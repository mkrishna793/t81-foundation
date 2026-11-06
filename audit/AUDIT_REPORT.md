# T81 Foundation — Independent Foundational & Algorithmic Audit

**Repository:** `mkrishna793/t81-foundation` @ `e978b0c`
**Audit date:** 2026-11-06
**Scope:** core numeric foundations (balanced-ternary representation, integer/
rational arithmetic), algorithm soundness, and the verification claims made by
the project.
**Out of scope:** JIT/LLVM lowering, networking, CanonFS storage format,
application-level policy engine behaviour.

---

## 0. How this audit was performed

Three *independent* verification layers were built and run. None of them reuse
the repository's own "reference" implementations, because those are what is
under test.

| Layer | Tool | What it establishes |
|---|---|---|
| **Oracle** | Python unbounded integers / `fractions.Fraction` | Ground truth. Every limb/int/fraction result is decoded to ℤ (or ℚ) from the *mathematical definition* `value = Σ tᵢ·3ⁱ` and compared. |
| **SMT** | **Z3 5.3.3** (`z3-solver`) | Universally-quantified claims proved by UNSAT of the negation; concrete counterexample models else. |
| **Proof assistant** | **Lean 4** (kernel, 4,196-file stdlib) | Machine-checked theorems about the shipped algorithms. **20 theorems, zero `sorry`, zero errors.** |

Reproduction:

```bash
# 1. empirical harness (C++20, no dependencies)
g++ -std=c++20 -O2 -Iinclude -o audit/build/findings audit/cpp/findings.cpp
T81_AXION_TRAP_STDERR=0 ./audit/build/findings

# 2. exact-integer oracle over the limb types
g++ -std=c++20 -O2 -Iinclude -o audit/build/limb_probe audit/cpp/limb_probe.cpp
./audit/build/limb_probe 400 987654321 > audit/logs/limb_probe.txt
python audit/smt/oracle.py < audit/logs/limb_probe.txt

# 3. SMT layer
python audit/smt/z3_audit.py

# 4. Lean 4 layer  (see audit/lean/README.md for toolchain notes)
node audit/lean/leanrun.js audit/lean/Stage1.lean
```

> **Toolchain note.** The sandbox only permits the allowlisted hosts, so
> `elan`/`release-assets.githubusercontent.com` are unreachable. Lean 4 was
> provisioned instead from the npm-distributed `lean4-wasm` bundle (the real
> Lean 4 kernel + stdlib compiled to WebAssembly), driven headlessly by
> `audit/lean/leanrun.js` under Node ≥ 22 (older V8 rejects the wasm's 201,110
> exports). `--server`/`--worker` mode is unsupported by that build, hence the
> ~2.5 min fixed startup; statements are therefore batched per file.

---

## 1. Executive summary

The **48-trit `T81Limb` arithmetic that the project actually relies on is
correct** — addition and wide multiplication were verified against exact
integers over 20,000 random pairs with zero failures, and the Kogge-Stone
carry-map formulation was verified sound over 200,000 pairs.

However the audit found **seven real defects**, all of which are *foundational
mathematical* problems rather than coding slips, plus one **documentation claim
that is not supported by any artefact in the repository**.

| # | Severity | Defect | Reachability |
|---|---|---|---|
| **F1** | High | `T81Limb27::operator*` Booth recoding table is mis-scaled by a factor 3 for 6 of 9 digit windows | Every 27×27→54 multiplication |
| **F2** | High | `T81Limb54::reference_mul` / `booth_mul_trits` emit **non-trits** (values up to ±16) and the wrong value in 300/300 cases | Benchmarks use it as the *correctness reference* |
| **F3** | High | `T81Limb54::operator*` (Karatsuba) is wrong — the radical sums overflow 27 trits and the carry is discarded | 200/200 random cases |
| **F4** | **Critical** | `T81Int<N>::operator*` overflows **silently** (wraps mod 3^N); `operator+` traps, `operator*` does not | `T81BigInt` and `T81Fraction<N>` are built on it |
| **F5** | **Critical** | `T81BigInt::operator+` single-limb fallback calls **`std::terminate()`** for any negative result with both operands ≥ 3^80 | Reachable from `T81BigInt` subtraction |
| **F6** | **Critical** | `T81Fraction<81>` arithmetic silently saturates to 0 | `T81Frac81` is **exported to Python** |
| **F7** | Medium | `noexcept` functions that throw → `std::terminate` (`to_double`, all fraction operators) | Any >40-trit numerator |
| **D1** | High (integrity) | `docs/research/formal-verification.md` claims core arithmetic is **"Status: Verified"**; no proof artefact exists anywhere in the repo | — |

The formal-verification claim is the most serious *integrity* finding: the
document states that `T81Int`, `T81BigInt`, `T81Fraction` and `T81Prob` have
"completed formal verification", and that extensive property testing "acts as a
practical proxy for full formal proofs". As §6 shows, neither is true — and
the arithmetic those documents certify is exactly where F4–F6 live.

---

## 2. Representation & encoding

Format under audit (`include/t81/packing.hpp`): a **tryte** is 3 trits packed
into `[-13,13]`, and a 48-trit limb is 16 trytes.

```
decode_tryte(v):  x = v + 13;  repeat 3× { digit = x%3 - 1; x /= 3 }
encode_tryte(d):  d0 + 3*d1 + 9*d2
```

**Verified correct** (Lean `decode_emits_legal_trits`, `decode_is_base3`,
`encode_decode_id`; Z3 `tryte.*`):

* every tryte decodes to three legal trits;
* `Σ dᵢ·3ⁱ = v − 13` — the decode is genuinely the balanced base-3 expansion;
* `encode ∘ decode = id` on the whole 27-element domain.

**But** `encode_tryte` is *total on ℤ³*, not a partial map on the legal-trit
cube. It is therefore **not injective** (Lean `enc_not_injective`):

```
encode(2, 0, 0) = 2 = encode(-1, 1, 0)
```

That is not a curiosity: it is the mechanism by which an unvalidated
intermediate digit silently becomes a legal-looking tryte. `T81Limb::from_trits`
calls `encode_tryte` on a bundle it has *not* validated as being in `{-1,0,1}`,
so this is the escape hatch through which the non-canonical trits of **F2**
propagate. Lean `enc_legal_trits_in_range` proves the fix is complete: restricting `encode_tryte` to
legal trits always yields a tryte in `[-13,13]`.

---

## 3. The carry chain: a *sound* algorithm applied outside its domain

This is the intellectual core of the audit, because the same mistake appears
three times in different disguises.

The shipped renormalisation step is:

```cpp
int sum = accum[i] + carry;
if (sum >=  2) { digit = sum - 3; carry =  1; }
else if (sum <= -2) { digit = sum + 3; carry = -1; }
else           { digit = sum;     carry =  0; }
```

This is *not* division by 3 — it is a truncated ±1 carry. It preserves value,
`s = digit + 3·carry` (Lean `shipStep_preserves_value`, Z3
`carry.value_preserved`), but it produces a legal trit **iff and only if the
incoming magnitude is ≤ 4**. Lean `shipStep_legal_of_small` proves the
implication inside the window, `shipStep_illegal_outside_window` gives witnesses
outside it (`5 → 2`, `−5 → −2`, `13 → 10`), and `shipped_requires_bound` states
the soundness contract as a conjunction. Z3 independently pins the exact
boundary and proves the floor-div-3 repair is legal on *every* integer:

```
carry.legal_exactly_on_window  PROVED
    first ill-formed output at s=5 -> digit 2; legal iff -4 <= s <= 4
```

So the C++ is not "wrong" in isolation — it is an algorithm with a hidden
precondition `|accum[i] + carry| ≤ 4` that **no call site checks**.

### 3.1 F2 — `T81Limb54::reference_mul` violates that precondition

`reference_mul` and `booth_mul_trits` build a full 54×54 schoolbook `accum`
(individual entries up to **±54**, measured up to 16 after one pass) and then
apply the ±1 step **once per trit**, twice. Measured against exact integers over
300 random 54-trit pairs:

```
value wrong             300/300
non-canonical output    298/300     (digits as large as 16; legal range is {-1,0,1})
```

The single-step rule tolerates `|s| ≤ 4`; the accumulator is an order of
magnitude larger. The fix is the `normalize_wide` loop already present in the
same file: carry `q = ⌊(s + sign(s))/3⌋`, `digit = s − 3q`.

### 3.2 F3 — `T81Limb54::karatsuba` loses the radical carry

```cpp
auto x0 = T81Limb27::from_low_27(x);   auto x1 = T81Limb27::from_high_27(x);
auto mid = T81Limb54((x0 + x1) * (y0 + y1));
auto z1  = (mid - z0) - z2;
```

`x0 + x1` is the sum of two 27-trit radicals and needs **28** trits. But
`T81Limb27::operator+` is `return addc(other).first;` — the 28th-trit carry-out
is **discarded**. This happens in **92 / 200** random cases, and the resulting
`operator*` is wrong in **200 / 200** cases.

Lean `radical_sum_overflows` proves the overflow is unavoidable:
`2·3²⁶ = 5083731656658` exceeds the 27-trit symmetric maximum
`(3²⁷−1)/2 = 3812798742493`; `radical_wrap_witness` shows the wrapped radical
becomes `2·3²⁶ − 3²⁷ = −3²⁶`, i.e. a different number; and
`wrapped_radical_changes_middle` pins that reduction mod `3²⁷` is the identity
exactly on the representable window.
The standard remedy is to carry the extra trit (9-tryte radicals must be
promoted to 10 trytes, or use `addc().second` explicitly).

> Note the same code exists for the 48-trit limb but is **not** broken there:
> `mul_wide` uses the 96-trit `normalize_wide` path and was verified exact over
> 20,000 pairs (C1). The defect is confined to the 54/27-trit classes.

### 3.3 C2 — what is *not* broken

The Kogge-Stone carry-map idea itself is sound. `detail::COMPOSITION_TABLE`,
`CARRY_FROM_ZERO` and the `addc` prefix scan agree with an independently
verified 48-trit ripple adder in **0 / 200,000** mismatches. The `addc` carry-out
is also exact over 200,000 exhaustive-style 27-trit pairs. **The defects are in
the scaling and renormalisation layers, not the parallel-prefix formulation.**

---

## 4. Silent overflow: the most dangerous class of defect

### 4.1 F4 — `T81Int<N>::operator*` has no guard

```cpp
friend constexpr T81Int operator*(const T81Int& a, const T81Int& b) {
  T81Int r;
  for (i…) { if (b[i]==0) continue; T81Int tmp = a; tmp <<= i; … r += tmp; }
  return r;                       // <-- no overflow check anywhere
}
```

`operator+` ends with `if (carry != 0) axion::trap_overflow(...)`, and `shift_left`
*silently zeroes* trits shifted past `N−1`. So the product is quietly reduced
mod 3^N with no diagnostic. Minimal reproduction:

```
T81Int<10>: 243 * 243 = 3^10  →  reads back as 0   (exact value 59049)
```

Lean `trunc_mul_loses_value` formalises it: multiplication is only correct under
`trits(a) + trits(b) ≤ N`, and `40+40 ≤ 81` holds while `¬(41+41 ≤ 81)` does not
— exactly the boundary the fast path depends on (`forty_safe`,
`fortyone_unsafe`). Z3 confirms the operating range is strictly narrower than the
representable range (`int.mul.needs_guard`). The 48-trit limb path *does* check
this (`T81BigInt::operator*` guards with `ta + tb <= 81`, and 41+41 = 82 > 81 is
correctly routed to the chunked path), which shows the team knew the invariant;
it is simply missing from `T81Int`.

### 4.2 F6 — user-visible silent wrong answers

`T81Fraction<81>` is `T81Int<81>`-backed and is **exported to Python**
(`lang/python/t81_python.cpp`, class `Fraction`). Its operators are exact only
within the 81-trit range, and there is no diagnostic on escape:

```
(3^45) * (3^45) = 3^90  →  numerator wraps to 0
```

A rational library that returns `0` for a large non-zero product is a silent
wrong answer at the API boundary. Either widen to `T81Fraction<162>`
(`T81Int` already supports `N ≤ 2048`), or make the operators return
`T81Result<T>`; a `noexcept` signature promising exactness is not tenable.

### 4.3 F7 — `noexcept` + throw = `std::terminate`

```cpp
[[nodiscard]] double to_double() const noexcept {
  const double n = static_cast<double>(num_.to_int64());   // throws if > 40 trits
```

`T81Int::to_int64()` throws `std::overflow_error` above 40 trits; inside a
`noexcept` function this is an immediate `std::terminate()` — **not** catchable.
Reproduced: a fraction with a 41-trit numerator aborts the process. The same
applies to every `T81Fraction<N>` `operator+/-/*`, all declared `noexcept`.

---

## 5. F5 — an invariant check that kills the process

`T81BigInt::normalize()` calls `verify_invariants()`, which requires the top
limb's sign trit to be non-negative, and enforces that with `std::terminate()`.
The single-limb fallback in `operator+` breaks that invariant:

```cpp
res.limbs_.push_back(a.negative_ ? -a.limbs_[0] : a.limbs_[0]);
Limb rhs = b.negative_ ? -b.limbs_[0] : b.limbs_[0];
res.limbs_[0] = res.limbs_[0] + rhs;
res.negative_ = false;          // <-- sign of the result is thrown away
res.normalize();                // <-- verify_invariants() -> std::terminate()
} catch (const std::overflow_error&) { /* fall through */ }
```

For a negative result this is unreachable-by-design: `std::terminate` is not an
exception, so the `catch` can never run. Lean `fallback_violates_invariant` / `fastpath_repairs_invariant` formalise it as
an invariant violation, and `three_pow_80_needs_81_trits` shows exactly when the
branch is taken (`3^80` is the smallest single-limb magnitude with 81 significant
trits).

Reachability, measured exhaustively over the operand grid:

| a \ b | 3⁴⁰ | 3⁷⁸ | 3⁷⁹ | 3⁸⁰ | 3⁸¹ |
|---|---|---|---|---|---|
| **3⁴⁰** | ok | ok | ok | **abort** | ok |
| **3⁷⁹** | ok | ok | ok | **abort** | ok |
| **3⁸⁰** | ok | ok | ok | **abort** | ok |
| **3⁸¹** | ok | ok | ok | ok | ok |

The failure is *not* monotone in magnitude (it needs exactly one operand with
trit 80 set and the other below it), which is why random testing has never hit
it — and why it will only ever appear in production as an unexplained abort.

Severity is high because it is an **availability** defect in the exact-arithmetic
core: `a − b` on two 80-trit integers kills the process instead of returning a
`T81Result` error or falling through to the chunked path.

---

## 6. Test-suite and verification-claim audit

### 6.1 The tests that "certify" the broken code

| Test | What it actually asserts |
|---|---|
| `test_t81_limb54_booth_mul.cpp` | `booth_mul(a,b) == reference_mul(a,b)` |
| `test_t81_limb_booth_mul.cpp` | `booth_mul(a,b) == reference_mul(a,b)` |
| `test_t81_limb_bohemian_mul.cpp` | `bohemian_mul(a,b) == reference_mul(a,b)` |

`reference_mul` is **F2** — the function that emits digits of ±16. These tests
assert that two implementations share a bug, and pass. This is the single
highest-leverage testing fix: assert against an *arithmetic identity* (an exact
integer oracle), never against a sibling implementation.

`test_t81_limb_mul_wide_high.cpp` is the counter-example done right: it compares
the high half against a locally re-derived normalisation rather than against
`mul_wide` itself — and that path passes, consistent with C1.

### 6.2 Coverage that does not exist

`tests/cpp/bigint_properties_test.cpp` labels its second phase:

> *"Large range (full int64 range …). This ensures we test deep multi-limb logic and slow-path division."*

But a `T81BigInt` limb is **81 trits ≈ 4.4×10³⁸**, and every `int64_t` needs at
most 41 trits:

```
int64 max = 9223372036854775807  <  3^41 = 36472996377170786403
⇒ every int64 value is a SINGLE limb
⇒ the "deep multi-limb" phase never leaves the single-limb fast path
```

Reaching the multi-limb path requires operands ≳ 10²⁰ × larger than any `int64`.
Consequently **F4, F5 and F6 are outside the reach of the entire existing
property-test suite**, by construction — not by accident.

`tests/cpp/t81int_properties_test.cpp` is structurally fine (ring axioms,
divisibility, explicit overflow traps for `+`) but clamps itself to
`[-1000, 1000]` "to avoid slow paths", so it never exercises a product that
overflows, which is exactly F4.

### 6.3 D1 — the formal-verification claim

`docs/research/formal-verification.md` states:

> **§1 Core Arithmetic Primitives — Status: Verified.** "We have completed formal verification for the core balanced ternary arithmetic types."
> "`T81Int`: Verified for ring properties… `T81BigInt`: Verified for … Karatsuba multiplication… `T81Fraction`: Verified…"
> "…supported by extensive property-based testing … which act as a practical proxy for full formal proofs in the current CI pipeline."

Audit result:

* There is **no** `.lean`, `.v`, `.thy`, `.tla` or `.smt2` file for the numeric
  core anywhere in the repository. A full-tree search finds only four
  `experiments/…/design_*.v` Verilog files (hardware feasibility sketches).
* "Acts as a proxy for formal proof" is **false as stated**, and demonstrably so:
  the property tests in question never leave a single limb (§6.2), so they
  cannot and do not certify `T81BigInt` Karatsuba (F3-adjacent), nor the
  overflow behaviour (F4), nor the invariant of F5.
* The claim is contradicted by §6 of this report, where the same three classes
  are shown to violate ring/representability properties under admissible inputs.

This should be downgraded to something like *"unverified; tested on a restricted
domain"* until proof artefacts exist. The audit deliverables in `audit/lean/`
are a starting point (23 machine-checked theorems, no `sorry`), and they already
contain the counter-theorems.

---

## 7. Algorithm audit

### 7.1 Karatsuba is engaged too late, and the transcode dominates once it is

`T81BigInt` stores 81-trit limbs but multiplies on a **27-trit chunk** view
(`to_chunks` / `from_chunks`). `karatsuba_mul_` only recurses when the chunk
count exceeds 32, i.e. above ~864 trits.

Measured cost per multiplication, ratio per doubling of operand size
(2 vCPU, gcc 13 `-O2`):

| trits | chunks | µs / mul | ratio per doubling | algorithm |
|---|---|---|---|---|
| 405 | 15 | 1145.9 | — | schoolbook |
| 810 | 30 | 3861.5 | 3.37 | schoolbook |
| 1620 | 60 | 14186.4 | 3.67 | Karatsuba |
| 3240 | 120 | 65901.8 | **4.65** | Karatsuba |

For reference: pure schoolbook is 4.00 per doubling, Karatsuba's asymptote is
`2^1.585 ≈ 2.99`. Two conclusions follow.

1. **Karatsuba never reaches its asymptote in the measured range.** The ratio
   *rises* to 4.65, worse than schoolbook, because each call re-transcodes both
   operands and the result (`to_chunks` is a table lookup per 2 packed bytes plus
   a per-chunk normalisation pass, `Θ(n)` with a large constant). At 3240 trits
   the transcode/allocation cost is a multiple of the arithmetic. The
   `std::vector<int128_t>` workspace allocated per recursion level compounds it.
2. **The threshold is on the wrong quantity.** It is expressed in *chunks* (32),
   which reads like "32 limbs" but is ~1/3 of that in limbs. Combined with the
   `+`-vs-`*` asymmetry (`operator+` also transcodes, but only to 27-trit chunks
   with no recursion), the practical crossover where Karatsuba repays its
   transcode cost is far above the current setting.

Actionable: keep values in chunk form at the type level (or cache the chunk
view), and re-derive the threshold empirically rather than from the textbook.

Also in this area:

* **`T81Int<N>::operator*` is not Karatsuba at all**, despite the file header
  claiming "Multiplication uses Karatsuba algorithm for large values". It is a
  shift-and-add loop over `N` trits — `Θ(N²)` with a full `T81Int` copy per
  set trit — and it is what drives `T81Fraction<N>`, so the exact-rational layer
  inherits the slow *and* unchecked path (F4/F6).
* **`booth_mul` is not a Booth multiplier.** `T81Limb54::booth_mul` computes the
  Booth-truncated product, then *recomputes the entire schoolbook product* and
  returns that when they disagree — 1.88× slower than the schoolbook reference
  for zero correctness gain, with a second copy of the buggy normalisation.

Per-operation cost at fixed limb width (ns):

```
T81Limb   op+ (addc)       145.0      T81Limb54 op+              175.0
T81Limb   mul_wide        1748.4      T81Limb54 op* (karatsuba) 1860.8
T81Limb   reference_mul   8170.4      T81Limb54 reference_mul   1804.4
                                      T81Limb54 booth_mul       3389.8
```

Note `T81Limb54` "Karatsuba" (1860.8) is **no faster** than the schoolbook
reference (1804.4) at 54 trits — it is below its own crossover, so the
complexity is paid for nothing.

### 7.2 `T81Limb::operator+` is a table-driven prefix scan; `addc` is correct
but the "unrolled Kogge-Stone" is a fixed 4-step scan

The implementation is correct (C2) but the comment "UNROLLED KOGGE-STONE" is
misleading: it performs exactly `log2(TRYTES)` sweeps with strides 1,2,4,8 for
16 trytes (5 would be needed for 32), so it is a correct Hillis-Steele/Brent-Kung
hybrid for the fixed width, not a general unrolled network. No action needed
beyond the comment, but worth noting the code is *shape-locked* to `TRYTES=16/18`
— `T81Limb54` repeats the same 5 blocks for 18 trytes, and the last sweep
(`i=16`) touches only two elements.

### 7.3 `div_mod` semantics

`T81Int<N>::div_mod` is correctly truncating-toward-zero and satisfies
`a = q·b + r` — verified over 3,002,000 exhaustive (a,b) pairs with zero failures,
plus the identity check in the existing suite. **No defect.** Worth recording
because it is often the first place such an audit finds a bug, and here it is
clean.

The `while (r >= v)` restoring loop is however `Θ(N²)` in the worst case and is
called by `T81Fraction::canonicalize()` (GCD) on every single fraction
construction — this, not the multiplication, is the real hot spot for
`T81Fraction`. `docs/explanation/USE_CASES.md` claims GCD normalisation is a hot
path in AI inference; if so, binary GCD (Stein) or Lehmer's algorithm is the
obvious win.

---

## 8. Findings summary table

| ID | Class | Location | Severity | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | algorithm | `T81Limb.hpp` `build_booth27_table` | High | findings F1: 6/9 windows mis-scaled | use `p·3^(2i)` weights; `shift∈{0,1,2}` trytes |
| F2 | algorithm | `T81Limb54::reference_mul`, `booth_mul_trits` | High | findings F2: 300/300 wrong, digits to ±16 | full div-3 carry (`normalize_wide`) |
| F3 | algorithm | `T81Limb54::karatsuba` | High | findings F3: 92/200 radical overflow, 200/200 wrong | keep `addc().second`; widen radicals |
| F4 | foundation | `T81Int::operator*` | **Critical** | findings F4: 3^10 reads as 0 | guard `trits(a)+trits(b) ≤ N`, else trap |
| F5 | foundation | `T81BigInt::operator+` fallback | **Critical** | findings F5: `std::terminate` | re-sign before `normalize`; make the catch reachable |
| F6 | foundation | `T81Fraction<81>` | **Critical** | findings F6: 3^90 → 0 | widen to 162; or return `T81Result` |
| F7 | foundation | `noexcept` + throwing conversion | Medium | findings F7 | drop `noexcept`, or make conversion total |
| D1 | integrity | `docs/research/formal-verification.md` | High | §6.3 | downgrade claim |

---

## 9. Recommended remediation order

1. **F5** — replace `std::terminate` in `verify_invariants()` with a recoverable
   error, and fix the fallback's sign handling. A process abort in an arithmetic
   core is the worst failure mode available.
2. **F4 / F6 / F7** — one coherent change: introduce a checked arithmetic
   contract (`T81Result`-returning operators, or a `checked_*` family) and stop
   advertising `noexcept` exactness. This closes three critical findings at once.
3. **F1–F3** — fix the 54/27-trit multiplication family, or delete it. It is
   benchmark-only today (`benchmarks/BM_LimbMul.cpp`); deleting the broken
   reference and its circular tests removes a trap for future contributors. If
   it is kept, its tests must assert against exact integers.
4. **Tests** — add an exact-integer oracle to CI for every arithmetic type
   (`audit/smt/oracle.py` is a working template) and add operands above 3^81 so
   the multi-limb path is actually reached.
5. **D1** — retitle the formal-verification document, and link `audit/lean/`
   as the first genuine (if partial) mechanisation.

---

## 10. Appendix — Lean 4 theorem index

All 20 theorems below are checked by the Lean 4 kernel: **0 errors, 0 `sorry`**.

| Theorem | Statement |
|---|---|
| `decode_emits_legal_trits` | `decode_tryte` emits only legal trits (exhaustive over all 27 trytes) |
| `decode_is_base3` | `decode_tryte` is the balanced base-3 expansion of `v−13` |
| `encode_decode_id` | `encode ∘ decode = id` on the whole tryte domain |
| `enc_not_injective` | `encode_tryte` is not injective on ℤ³ — digit aliasing (aliases `2` onto `(-1,+1)`) |
| `enc_legal_trits_in_range` | restricted to legal trits, `encode_tryte` lands in `[-13,13]` |
| `shipStep_preserves_value` | shipped carry step preserves value: `s = digit + 3·carry` |
| `shipStep_legal_of_small` | shipped carry step emits a legal trit whenever `\|s\| ≤ 4` |
| `shipStep_illegal_outside_window` | witnesses of illegality: `5→2`, `−5→−2`, `13→10` |
| `shipped_requires_bound` | the sound contract is exactly "inputs stay in the window", and an unbounded violation exists |
| `addDigit_legal` | `T81Int` column adder emits a legal trit for legal inputs |
| `add_can_carry_out` | carry-out `±1` is reachable — overflow is real, not hypothetical |
| `trunc_mul_loses_value` | `3^10 mod 3^10 = 0` while `3^10 ≠ 0`: truncating multiply loses value |
| `forty_safe` | `40 + 40 ≤ 81` (two 40-trit operands always fit the 81-trit limb) |
| `fortyone_unsafe` | `¬(41 + 41 ≤ 81)` — the boundary `T81BigInt` must route to the slow path |
| `radical_sum_overflows` | `2·3^26 > (3^27−1)/2`: 27-trit radicals cannot be summed in 27 trits |
| `wrapped_radical_changes_middle` | reduction mod `3^27` is the identity exactly on the representable window |
| `radical_wrap_witness` | wrap witness `2·3^26 − 3^27 = −3^26` |
| `fallback_violates_invariant` | `(magnitude, sign)` of the `T81BigInt` fallback violates the invariant |
| `fastpath_repairs_invariant` | the fast path re-signs and does not |
| `three_pow_80_needs_81_trits` | `3^80` is the smallest 81-trit magnitude — exactly the branch condition |

Two further claims are **delegated to the SMT layer**, because this Lean build
cannot close universally-quantified `Int`-division goals without extra `Int.ediv`
lemmas: column value-preservation (`int.add_column.value`) and the legality of
the floor-div-3 repair for all integers (`int.add_column.legal`,
`realStep`). Z3 proves both by bit-blasting, so they remain machine-checked.

## 11. Appendix — audit artefacts

```
audit/
├── AUDIT_REPORT.md              (this document)
├── README.md                    (how to reproduce)
├── cpp/
│   ├── findings.cpp             self-checking defect harness (F1–F7, C1–C2)
│   ├── limb_probe.cpp           emits limb results for the exact-integer oracle
│   ├── int_probe.cpp            T81Int differential probe
│   └── core_probe.cpp           T81BigInt / T81Fraction differential probe
├── smt/
│   ├── oracle.py                independent ℤ/ℚ oracle
│   ├── z3_audit.py              14 SMT checks (PROVED / CONFIRMED)
│   └── z3_results.json
├── lean/
│   ├── Stage1.lean              23 kernel-checked theorems
│   ├── leanrun.js               headless Lean 4 runner
│   └── README.md                toolchain provisioning notes
└── logs/                        captured runs
```
