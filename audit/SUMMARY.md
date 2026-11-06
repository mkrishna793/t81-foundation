# Audit — one-page summary

**Repository:** `mkrishna793/t81-foundation` @ `e978b0c` · **Date:** 2026-11-06

Run everything: `./audit/verify.sh`  (last run: **rc=0**, all layers green)

| Layer | Result |
|---|---|
| C++ defect harness | **7 defects** reproduced deterministically (F1–F7), 2 "clean" controls (C1–C2) |
| Python exact-integer oracle | 48-trit limb: **all checks pass**; 54/27-trit: **fail** (see F1–F3) |
| Z3 SMT | **9 claims PROVED**, 5 counterexamples CONFIRMED |
| Lean 4 kernel | **20 theorems, 0 errors, 0 `sorry`** |

## The seven defects

| ID | Severity | One line |
|---|---|---|
| F1 | High | `T81Limb27::operator*` Booth recoding table mis-scaled by 3 in 6/9 windows |
| F2 | High | `T81Limb54::reference_mul` emits digits up to ±16 (not trits) and the wrong value, 300/300 |
| F3 | High | `T81Limb54::karatsuba` drops the 28th-trit radical carry → wrong in 200/200 |
| F4 | **Critical** | `T81Int<N>::operator*` overflows **silently** (`3^10` reads as `0`) |
| F5 | **Critical** | `T81BigInt` subtraction of two 81-trit values calls **`std::terminate()`** |
| F6 | **Critical** | `T81Fraction<81>` silently saturates: `3^45 · 3^45 → 0` (exported to Python) |
| F7 | Medium | `noexcept` + throwing conversion → `std::terminate` |

Plus **D1**: `docs/research/formal-verification.md` claims core arithmetic is
"Status: Verified" — there is no proof artefact for it anywhere in the repo, and
the property tests it cites only exercise a **single limb** (every `int64` is
≤ 41 trits; a limb is 81), so they cannot reach F4–F6.

## What is *correct* (worth saying)

* `T81Limb` (48-trit) add and `mul_wide`: exact over 20,000 pairs, 0 failures.
* The Kogge-Stone carry-map prefix scan: sound over 200,000 pairs.
* `T81Int::div_mod`: `a = q·b + r` over 3,002,000 exhaustive pairs, 0 failures.
* The tryte codec: decode is the true base-3 expansion; round-trip is exact.

## Root cause, in one sentence

The shipped renormalisation step is a truncated ±1 carry that is only valid while
the accumulator stays in `[-4, 4]`; it is applied to accumulators that reach ±54
(F2), to 27-trit radicals that need 28 (F3), and to multiplication paths with no
overflow guard at all (F4) — so the arithmetic is correct exactly where the
precondition happens to hold (the verified 48-trit path) and wrong everywhere it
is not.
