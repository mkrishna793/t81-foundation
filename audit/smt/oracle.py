#!/usr/bin/env python3
"""
Independent oracle for T81 limb arithmetic.

Ground truth = exact unbounded integer arithmetic.  Nothing in this file reuses
any T81 code path: balanced-ternary encoding/decoding is written from the
mathematical definition
      value(t) = sum_i t_i * 3^i ,  t_i in {-1,0,1}
"""
import sys

def to_val(trits):
    return sum(int(t) * (3 ** i) for i, t in enumerate(trits))

def balanced(x, n):
    """Canonical balanced ternary of x in n positions (exact, no wraparound)."""
    out = []
    for _ in range(n):
        r = x % 3            # python % is non-negative for positive modulus
        if r == 0:
            out.append(0)
        elif r == 1:
            out.append(1); x -= 1
        else:                # r == 2  -> -1 with carry
            out.append(-1); x += 1
        x //= 3
    return out, x            # x = residual that did not fit in n trits

def wrap(x, n):
    """Two's-complement-analogue reduction modulo 3^n into symmetric range."""
    m = 3 ** n
    x %= m
    if x > (m - 1) // 2:
        x -= m
    return x

class Report:
    def __init__(self):
        self.fail = {}
        self.n = {}
    def add(self, name, ok, detail=""):
        self.n[name] = self.n.get(name, 0) + 1
        if not ok:
            self.fail.setdefault(name, []).append(detail)

def canonical(trits):
    return all(-1 <= int(t) <= 1 for t in trits)

def check(stream, rep):
    """
    Parse the probe stream and verify every emitted vector against the oracle.
    """
    tok = stream.split("\n")
    cur = {}
    idx = 0
    while idx < len(tok):
        line = tok[idx].strip(); idx += 1
        if not line or line.startswith("#"):
            continue
        if line == "---":
            run_block(cur, rep); cur = {}; continue
        parts = line.split()
        cur[parts[0]] = parts[1:]

def run_block(b, rep):
    if not b:
        return
    def V(k):
        return [int(x) for x in b[k]]

    # ---- tryte table ----
    if "TRYTE" in b:
        v = int(b["TRYTE"][0]); d = [int(x) for x in b["TRYTE"][1:4]]; back = int(b["TRYTE"][4])
        rep.add("tryte.decode.in_range", all(abs(x) <= 1 for x in d), f"{v}->{d}")
        rep.add("tryte.roundtrip", to_val(d) == v, f"{v}->{d} val={to_val(d)}")
        rep.add("tryte.encode", back == v, f"{v}->{back}")
        return

    # ---- 48-trit limb ----
    if "ENC_A" in b:
        A, B = V("ENC_A"), V("ENC_B")
        rep.add("limb48.encode.canonical_a", canonical(A), "non-canonical trit")
        rep.add("limb48.encode.canonical_b", canonical(B), "non-canonical trit")
        a, bb = to_val(A), to_val(B)

        S = V("ADD")
        rep.add("limb48.add.canonical", canonical(S), "non-canonical trit")
        rep.add("limb48.add.mod3_48", to_val(S) == wrap(a + bb, 48),
                f"{a}+{bb} got {to_val(S)} want {wrap(a+bb,48)}")
        carry = int(b["CARRY"][0])
        rep.add("limb48.addc.carry", to_val(S) + carry * (3 ** 48) == a + bb,
                f"a+b={a+bb} sum+carry*3^48={to_val(S)+carry*3**48} carry={carry}")

        LO, HI = V("MUL_LOW"), V("MUL_HIGH")
        rep.add("limb48.mul_wide.canonical", canonical(LO) and canonical(HI), "non-canonical trit")
        prod = to_val(LO) + to_val(HI) * (3 ** 48)
        rep.add("limb48.mul_wide.exact", prod == a * bb,
                f"got {prod} want {a*bb}")
        rep.add("limb48.mul_trunc.mod3_48", to_val(V("MUL_TRUNC")) == wrap(a * bb, 48),
                f"got {to_val(V('MUL_TRUNC'))} want {wrap(a*bb,48)}")
        rep.add("limb48.booth.mod3_48", to_val(V("BOOTH")) == wrap(a * bb, 48),
                f"got {to_val(V('BOOTH'))} want {wrap(a*bb,48)}")
        return

    # ---- 54-trit limb ----
    if "L54_A" in b:
        A, B = V("L54_A"), V("L54_B")
        rep.add("limb54.encode.canonical", canonical(A) and canonical(B), "non-canonical trit")
        a, bb = to_val(A), to_val(B)
        S = V("L54_ADD")
        rep.add("limb54.add.canonical", canonical(S), "non-canonical trit")
        carry = int(b["L54_CARRY"][0])
        rep.add("limb54.add.exact", to_val(S) + carry * (3 ** 54) == a + bb,
                f"{a}+{bb} vs {to_val(S)}+{carry}*3^54")
        D = V("L54_SUB")
        rep.add("limb54.sub.canonical", canonical(D), "non-canonical trit")
        rep.add("limb54.sub.mod3_54", to_val(D) == wrap(a - bb, 54),
                f"got {to_val(D)} want {wrap(a-bb,54)}")
        for tag, name in (("L54_KARATSUBA", "limb54.mul_ans=operator*"),
                          ("L54_BOOTH", "limb54.booth_mul"),
                          ("L54_REFMUL", "limb54.reference_mul")):
            T = V(tag)
            rep.add(name + ".canonical", canonical(T), "non-canonical trit")
            rep.add(name + ".mod3_54", to_val(T) == wrap(a * bb, 54),
                    f"got {to_val(T)} want {wrap(a*bb,54)}")
        return

    # ---- 27x27 -> 54 ----
    if "L27_A" in b:
        A, B = V("L27_A"), V("L27_B")
        a = to_val(A[:27]); bb = to_val(B[:27])
        T = V("L27_MUL")
        rep.add("limb27.mul.canonical", canonical(T), "non-canonical trit")
        rep.add("limb27.mul.exact", to_val(T) == a * bb,
                f"got {to_val(T)} want {a*bb}")
        return

def main():
    data = sys.stdin.read()
    rep = Report()
    check(data, rep)
    print("=" * 78)
    print(f"{'CHECK':<42}{'RUNS':>7}{'FAIL':>7}  VERDICT")
    print("=" * 78)
    bad = 0
    for k in sorted(rep.n):
        f = len(rep.fail.get(k, []))
        if f:
            bad += 1
        print(f"{k:<42}{rep.n[k]:>7}{f:>7}  {'FAIL' if f else 'ok'}")
    print("=" * 78)
    if bad:
        print("\nFIRST COUNTEREXAMPLES")
        for k in sorted(rep.fail):
            print(f"\n  {k}  ({len(rep.fail[k])} failures)")
            for d in rep.fail[k][:3]:
                print(f"    - {d}")
    sys.exit(1 if bad else 0)

if __name__ == "__main__":
    main()
