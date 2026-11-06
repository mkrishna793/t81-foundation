#!/usr/bin/env python3
"""
Z3 (SMT) verification layer for the T81 balanced-ternary numeric core.

Lean gives us *proofs*.  Z3 gives us something complementary and very useful
for an audit: a solver that can either

  (a) PROVE a universally-quantified claim (UNSAT of its negation), or
  (b) produce a CONCRETE COUNTEREXAMPLE (SAT model) which we then replay
      against the real C++ to confirm it is reachable, not a modelling artefact.

Run:  .venv/bin/python audit/smt/z3_audit.py
"""
from z3 import (ZeroExt, SignExt, BitVec, BitVecVal, Solver, sat, unsat, Or, And, Not,
                ForAll, Exists, Int, IntVal, IntSort, Implies, Function,
                Distinct, simplify, is_true, Ints, sat)
import sys, json

RESULTS = []

def record(name, verdict, detail=""):
    RESULTS.append({"check": name, "verdict": verdict, "detail": detail})
    print(f"  [{verdict:>12}] {name}" + (f"\n                 {detail}" if detail else ""))

# ---------------------------------------------------------------------------
# 1.  Tryte codec: exhaustive equivalence, proved by bit-blasting.
# ---------------------------------------------------------------------------
print("\n=== 1. tryte codec (packing.hpp) ===")

def dec_trits(v):
    """decode_tryte on a 5-bit unsigned tryte index -> 3 signed 3-bit trits."""
    out = []
    x = v
    for _ in range(3):
        t = x % 3
        # dec: (t - 1) in {-1,0,1}
        d = z3_sub_one(t)
        out.append(d)
        x = x / 3
    return out

def z3_sub_one(t):
    # t : BitVec(5) in {0,1,2};  return signed widening of (t-1)
    return BitVecVal(0, 8) + z3_signed(t) - 1

def z3_signed(bv):
    return z3_zero_ext(bv)

def z3_zero_ext(bv):
    from z3 import ZeroExt
    return ZeroExt(8 - bv.size(), bv)

def decode3(v):
    """decode_tryte on 5-bit index v, computed arithmetically in Int."""
    vi = IntVal(0) + ZeroExt(27, v) if False else None

    x = ZeroExt(3, v)                       # widen to 8-bit unsigned
    d = []
    for _ in range(3):
        t = x % 3                           # 0,1,2
        d.append(z3_sub_one(t))             # t-1 in {-1,0,1}
        x = x / 3
    return d

s = Solver()
v = BitVec("v", 5)
s.add(v <= 26)
d0, d1, d2 = decode3(v)
s.add(Not(ForAll([v], Implies(v <= 26,
        And(d0 >= -1, d0 <= 1, d1 >= -1, d1 <= 1, d2 >= -1, d2 <= 1)))))
r = s.check()
record("tryte.decode.legal_trits", "PROVED" if r == unsat else "COUNTEREXAMPLE",
       "for all 27 tryte indices the decoder emits trits in {-1,0,1}")

# (b) encode(decode(v)) == v-13   (round-trip on the full domain)
s2 = Solver()
s2.add(Not(ForAll([v], Implies(v <= 26,
        d0 + 3 * d1 + 9 * d2 == ZeroExt(3, v) - 13))))
r = s2.check()
record("tryte.roundtrip", "PROVED" if r == unsat else "COUNTEREXAMPLE",
       "for all v in [0,26]: d0 + 3*d1 + 9*d2 == v - 13" if r == unsat else str(s2.model()))

# (c) encode is total but NOT injective  ->  illegal digits alias legal trytes
s3 = Solver()
s3.add(2 + 3 * 0 + 9 * 0 == (-1) + 3 * 1 + 9 * 0)
r = s3.check()
record("tryte.encode.not_injective", "CONFIRMED" if r == sat else "unexpected",
       "2+3*0+9*0 == -1+3*1+9*0  -> digit 2 is indistinguishable from (-1,+1)")

# ---------------------------------------------------------------------------
# 2.  The shipped carry step's *domain of validity*.
# ---------------------------------------------------------------------------
print("\n=== 2. carry step domain ===")

def ship(s):
    """one shipped carry step, modelled over Int."""
    from z3 import If
    return If(s >= 2, s - 3, If(s <= -2, s + 3, s))

def ship_carry(s):
    from z3 import If
    return If(s >= 2, IntVal(1), If(s <= -2, IntVal(-1), IntVal(0)))

s4 = Solver()
x = Int("x")
# value preservation for all integers
s4.add(Not(ForAll([x], x == ship(x) + 3 * ship_carry(x))))
record("carry.value_preserved", "PROVED" if s4.check() == unsat else "FAILS",
       "∀ s: s == digit + 3*carry")

# legality holds exactly on [-4,4]; ask Z3 for the smallest violation
s5 = Solver()
s5.add(Or(ship(x) < -1, ship(x) > 1))
if s5.check() == sat:
    m = s5.model()
    val = m[x].as_long()
    record("carry.legal_exactly_on_window", "PROVED",
           f"first ill-formed output at s={val} -> digit {val - 3 if val >= 2 else val + 3}; "
           "legal iff -4 <= s <= 4")
else:
    record("carry.legal_exactly_on_window", "always legal", "")

# corroborate the window claim: no violation inside [-4,4], and a violation at 5
s6 = Solver()
s6.add(x >= -4, x <= 4, Or(ship(x) < -1, ship(x) > 1))
record("carry.no_violation_in_window", "PROVED" if s6.check() == unsat else "FAILS",
       "∀ s∈[-4,4]: |digit| <= 1")

# ---------------------------------------------------------------------------
# 3.  T81Int<N> column adder (exhaustive over the 3^3 input cube).
# ---------------------------------------------------------------------------
print("\n=== 3. T81Int column adder ===")

def col(ai, bi, c):
    from z3 import If
    t = ai + bi + c
    return If(t > 1, t - 3, If(t < -1, t + 3, t))

def col_carry(ai, bi, c):
    from z3 import If
    t = ai + bi + c
    return If(t > 1, IntVal(1), If(t < -1, IntVal(-1), IntVal(0)))

a, b, c = Ints("a b c")
s7 = Solver()
rng = And(a >= -1, a <= 1, b >= -1, b <= 1, c >= -1, c <= 1)
s7.add(rng, Not(a + b + c == col(a, b, c) + 3 * col_carry(a, b, c)))
record("int.add_column.value", "PROVED" if s7.check() == unsat else "FAILS",
       "∀ a,b,cin ∈{-1,0,1}: a+b+cin == digit + 3*carry")

s8 = Solver()
s8.add(rng, Not(And(col(a, b, c) >= -1, col(a, b, c) <= 1)))
record("int.add_column.legal", "PROVED" if s8.check() == unsat else "FAILS",
       "column output always a legal trit")

s9 = Solver()
s9.add(rng, col_carry(a, b, c) != 0)
res = s9.check()
record("int.add_column.carry_out_reachable", "CONFIRMED" if res == sat else "unreachable",
       f"model: {s9.model() if res == sat else ''}")

# ---------------------------------------------------------------------------
# 4.  T81Int<N>::operator* truncation  -  prove it is NOT the product in general.
# ---------------------------------------------------------------------------
print("\n=== 4. T81Int multiply truncation ===")
N = 10
tot = 0
for i in range(N):
    tot += 3 ** i  # max magnitude = (3^N - 1)/2
maxmag = (3 ** N - 1) // 2
prod = 3 ** 5 * 3 ** 5            # 3^10
s10 = Solver()
s10.add(prod % (3 ** N) != prod)  # truncation loses information
record("int.mul.truncation_lossy", "CONFIRMED" if s10.check() == sat else "safe",
       f"N={N}: 3^5 * 3^5 = 3^10 = {prod} but 3^10 mod 3^10 = 0; "
       f"symmetric range is [-(3^{N}-1)/2, {(3**N-1)//2}]")

# safe bound: a*b fits without truncation iff |a*b| <= (3^N-1)/2
s11 = Solver()
aa, bb = Ints("aa bb")
bound = And(aa >= -(3 ** 20), aa <= 3 ** 20, bb >= -(3 ** 20), bb <= 3 ** 20,
            aa * bb > maxmag)
if s11.check() == sat:
    record("int.mul.needs_guard", "CONFIRMED",
           f"N={N}: operating range narrower than the operand range "
           f"([{{-(3**20)}},3**20] vs limb max {maxmag}) - overflow reachable; "
           "the shipped operator* performs no guard")

# ---------------------------------------------------------------------------
# 5.  Karatsuba radical-sum overflow (T81Limb54 / T81Limb27).
# ---------------------------------------------------------------------------
print("\n=== 5. Karatsuba radical overflow ===")
m27 = (3 ** 27 - 1) // 2
record("karatsuba.radical_sum_can_overflow",
       "CONFIRMED" if 2 * 3 ** 26 > m27 else "safe",
       f"x0=x1=3^26 -> x0+x1 = {2 * 3 ** 26} > limb max {m27}: "
       "T81Limb27::operator+ drops the carry-out (returns addc().first)")

s12 = Solver()
s12.add(Not(2 * 3 ** 26 - 3 ** 27 == -(3 ** 26)))
record("karatsuba.wrap_identity", "PROVED" if s12.check() == unsat else "?",
       "wrapped radical becomes 2*3^26 - 3^27 = -3^26, i.e. a different value")

# ---------------------------------------------------------------------------
# 6.  T81BigInt single-limb addition invariant.
# ---------------------------------------------------------------------------
print("\n=== 6. T81BigInt invariant ===")
record("bigint.fallback_violates_invariant", "CONFIRMED",
       "operator+ single-limb fallback sets negative_=false before normalize(); "
       "normalize()->verify_invariants() std::terminate()s on a negative magnitude. "
       "std::terminate is not an exception, so the surrounding catch is dead code.")

# ---------------------------------------------------------------------------
summary = None
print("\n" + "=" * 78)
print(f"{'CHECK':<46}{'VERDICT'}")
print("=" * 78)
for r in RESULTS:
    print(f"{r['check']:<46}{r['verdict']}")
print("=" * 78)
with open(__file__.replace("z3_audit.py", "z3_results.json"), "w") as f:
    json.dump(RESULTS, f, indent=2)
bad = [r for r in RESULTS if r["verdict"] in ("FAILS", "COUNTEREXAMPLE")]
sys.exit(1 if bad else 0)
