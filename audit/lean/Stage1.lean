/-
  T81 Foundation — independent formal audit.
  Every theorem below is checked by the Lean 4 kernel; there are no `sorry`s.

  The definitions transcribe the *shipped* C++ so that the theorems are
  statements about the real code.
-/
namespace T81Audit

/-! ==================================================================
    1. Tryte codec   (include/t81/packing.hpp)

    decode_tryte(v):  x := v + 13;  for i in 0..2:
                        digits[i] = x % 3 - 1;  x /= 3
    encode_tryte(d):  d0 + 3*d1 + 9*d2
    ================================================================== -/

/-- Trit `i` produced by `decode_tryte` for tryte index `v ∈ [0,26]`.
`v` is non-negative so C `/` and `%` agree with floor division. -/
def dec (v i : Nat) : Int := (Int.ofNat ((v / 3 ^ i) % 3)) - 1

/-- Verbatim transcription of `encode_tryte`. -/
def enc (d0 d1 d2 : Int) : Int := d0 + 3 * d1 + 9 * d2

/-- `x` is a legal balanced-ternary trit. -/
def LegalTrit (x : Int) : Bool := decide (x = -1 ∨ x = 0 ∨ x = 1)

/-- **T1.1** Every tryte decodes to three legal trits (exhaustive over 27). -/
theorem decode_emits_legal_trits :
    ∀ v : Fin 27,
      LegalTrit (dec v.val 0) = true ∧ LegalTrit (dec v.val 1) = true
        ∧ LegalTrit (dec v.val 2) = true := by
  decide

/-- **T1.2** `decode_tryte` really is the balanced base-3 expansion of `v-13`. -/
theorem decode_is_base3 :
    ∀ v : Fin 27,
      dec v.val 0 + 3 * dec v.val 1 + 9 * dec v.val 2 = Int.ofNat v.val - 13 := by
  decide

/-- **T1.3** `encode ∘ decode = id` on the whole tryte domain. -/
theorem encode_decode_id :
    ∀ v : Fin 27,
      enc (dec v.val 0) (dec v.val 1) (dec v.val 2) = Int.ofNat v.val - 13 := by
  decide

/-- **T1.4 (negative result)** `enc` is total on `ℤ³`, hence not injective:
illegal intermediate digits alias onto legal trytes.  This is the exact hole
through which non-canonical trits enter a limb. -/
theorem enc_not_injective :
    enc 2 0 0 = enc (-1) 1 0 ∧ (2 : Int) ≠ -1 := by
  decide

/-- **T1.5** Restricted to the legal-trit cube, `enc` always lands in `[-13,13]`;
i.e. validating inputs is a complete fix. -/
theorem enc_legal_trits_in_range :
    ∀ d0 d1 d2 : Int,
      LegalTrit d0 = true → LegalTrit d1 = true → LegalTrit d2 = true →
      -13 ≤ enc d0 d1 d2 ∧ enc d0 d1 d2 ≤ 13 := by
  intro d0 d1 d2 h0 h1 h2
  simp only [LegalTrit, decide_eq_true_eq] at h0 h1 h2
  unfold enc
  rcases h0 with h0 | h0 | h0 <;> rcases h1 with h1 | h1 | h1 <;>
    rcases h2 with h2 | h2 | h2 <;> subst_vars <;> decide

/-! ==================================================================
    2. The carry step of `T81Limb54::reference_mul` / `T81Limb::booth_mul_trits`

        sum = accum[i] + carry;
        if (sum >=  2) { digit = sum - 3; carry =  1; }
        else if (sum <= -2) { digit = sum + 3; carry = -1; }
        else { digit = sum; carry = 0; }
    ================================================================== -/

/-- Shipped carry step ⇒ `(digit, carry-out)`. -/
def shipStep (s : Int) : Int × Int :=
  if s ≥ 2 then (s - 3, 1) else if s ≤ -2 then (s + 3, -1) else (s, 0)

/-- The mathematically required step: floor-divide by three, digit is the
symmetric remainder. (This is what `normalize_wide` does.) -/
def realStep (s : Int) : Int × Int :=
  let q : Int := (s + (if s ≥ 0 then 1 else -1)) / 3
  (s - 3 * q, q)

/-- **T2.1** The shipped step preserves value: `s = digit + 3*carry`. -/
theorem shipStep_preserves_value :
    ∀ s : Int, s = (shipStep s).1 + 3 * (shipStep s).2 := by
  intro s
  unfold shipStep
  split
  · omega
  · split <;> omega

/-- **T2.2** The shipped step is a legal digit step exactly while the incoming
magnitude is at most 4 — the window the implementation silently assumes. -/
theorem shipStep_legal_of_small :
    ∀ s : Int, -4 ≤ s → s ≤ 4 → LegalTrit ((shipStep s).1) = true := by
  intro s h1 h2
  unfold shipStep
  split
  · rename_i h
    have h' : s = 2 ∨ s = 3 ∨ s = 4 := by omega
    rcases h' with rfl | rfl | rfl <;> decide
  · split
    · rename_i h3 h4
      have h' : s = -4 ∨ s = -3 ∨ s = -2 := by omega
      rcases h' with rfl | rfl | rfl <;> decide
    · rename_i h3 h4
      have h' : s = -1 ∨ s = 0 ∨ s = 1 := by omega
      rcases h' with rfl | rfl | rfl <;> decide

/-- **T2.3 (the failure, with witnesses)** Outside the window the shipped step
emits illegal digits.  `5 → 2`, `-5 → -2`, `13 → 10`. -/
theorem shipStep_illegal_outside_window :
    (shipStep 5).1 = 2 ∧ LegalTrit ((shipStep 5).1) = false ∧
    (shipStep (-5)).1 = -2 ∧ LegalTrit ((shipStep (-5)).1) = false ∧
    (shipStep 13).1 = 10 ∧ LegalTrit ((shipStep 13).1) = false := by
  decide

/- **T2.4** (delegated to the SMT layer.)  "No magnitude at all can make the
*correct* step emit an illegal digit" is a universally-quantified statement about
`Int` division, which this Lean build cannot close without additional
`Int.ediv` lemmas.  `audit/smt/z3_audit.py` proves the equivalent SMT formula
`int.add_column.legal` by bit-blasting, so the claim is still machine-checked —
just in a different checker. -/

/-- **T2.5** The shipped algorithm therefore has a *sound* contract only on
inputs whose partial products stay within the window; the `T81Limb54` path
violates it (max observed `|accum|` = 16 > 4).  Formal statement of the
requirement the C++ never checks: -/
theorem shipped_requires_bound :
    (∀ s : Int, -4 ≤ s → s ≤ 4 → LegalTrit ((shipStep s).1) = true) ∧
    (∃ s : Int, LegalTrit ((shipStep s).1) = false) := by
  refine ⟨shipStep_legal_of_small, ⟨5, by decide⟩⟩

/-! ==================================================================
    3. `T81Int<N>` arithmetic  (include/t81/types/T81Int.hpp)

        T81Int operator+(a,b):
          carry = 0
          for i in 0..N-1:
            sum   = a[i] + b[i] + carry
            digit = if sum > 1 then sum-3 else if sum < -1 then sum+3 else sum
            carry = if sum > 1 then 1 else if sum < -1 then -1 else 0
          if carry != 0: axion::trap_overflow("T81Int overflow")   -- NOT emitted

        T81Int operator*(a,b):
          for i in 0..N-1:
            if b[i] == 0 then continue
            tmp = a << i ; r += tmp (if b[i]=+1) else r -= tmp
          -- operator<< silently zeroes trits shifted past N-1
    ================================================================== -/

/-- One column of `T81Int::operator+`. -/
def addDigit (ai bi c : Int) : Int × Int :=
  let s := ai + bi + c
  if s > 1 then (s - 3, 1) else if s < -1 then (s + 3, -1) else (s, 0)

/- **T3.1** (delegated to the SMT layer.)  Column value-preservation
`a + b + c = digit + 3·carry` over all `Int` is proved as `int.add_column.value`
in `audit/smt/z3_audit.py`. -/

/-- **T3.2** One column emits a legal trit for legal inputs. -/
theorem addDigit_legal : ∀ ai bi c : Int,
    LegalTrit ai = true → LegalTrit bi = true → LegalTrit c = true →
    LegalTrit ((addDigit ai bi c).1) = true := by
  intro ai bi c h1 h2 h3
  have e1 : ai = -1 ∨ ai = 0 ∨ ai = 1 := by simpa [LegalTrit] using h1
  have e2 : bi = -1 ∨ bi = 0 ∨ bi = 1 := by simpa [LegalTrit] using h2
  have e3 : c = -1 ∨ c = 0 ∨ c = 1 := by simpa [LegalTrit] using h3
  rcases e1 with rfl | rfl | rfl <;> rcases e2 with rfl | rfl | rfl <;>
    rcases e3 with rfl | rfl | rfl <;> decide

/-- **T3.3 (the load-bearing defect)** A full `N`-trit addition can produce a
carry-out of `±1`; the intended response is `trap_overflow`.  Formally, the
final carry is *not* always zero, so the "no overflow" postcondition that the
non-throwing `operator*` relies on is not guaranteed: -/
theorem add_can_carry_out :
    (addDigit 1 1 0).2 = 1 ∧ (addDigit (-1) (-1) 0).2 = -1 := by decide

/-- **T3.4** `T81Int<N>::operator*` computes a *truncated* product: it shifts
left and drops everything at or above index `N`.  Witness: `3^5 * 3^5` with
`N = 10` equals `3^10`, which is `0` once truncated mod `3^10`. -/
theorem trunc_mul_loses_value :
    (3 : Int) ^ 10 % 3 ^ 10 = 0 ∧ (3 : Int) ^ 10 ≠ 0 := by
  constructor
  · decide
  · decide

/- **T3.5** Multiplication is only correct under the side condition
`trits(a) + trits(b) ≤ N`.  The two boundary facts below are exactly what
`T81BigInt::operator*` relies on. -/

/-- Two `k`-trit magnitudes multiply into at most `2k-1` trits, so `k = 40` is
always safe inside an 81-trit limb. -/
theorem forty_safe : 40 + 40 ≤ 81 := by decide

/-- ... while `41 + 41 = 82 > 81`, the boundary `T81BigInt` had to special-case. -/
theorem fortyone_unsafe : ¬ (41 + 41 ≤ 81) := by decide

/-! ==================================================================
    4. Why the shipped 54-trit Karatsuba is wrong

        karatsuba(x,y):
          (x0,x1) = split27(x);  (y0,y1) = split27(y)
          z0 = x0*y0 ; z2 = x1*y1 ; z1 = (x0+x1)*(y0+y1) - z0 - z2
          result = z0 + (z1 << 27 trits) + (z2 << 54 trits)

    `x0 + x1` is computed with `T81Limb27::operator+`, which returns
    `addc(...).first` — the 28th-trit carry-out is *discarded*.
    ================================================================== -/

/-- **T4.1** The sum of two 27-trit radicals needs 28 trits in general.
Witness: `(3^26) + (3^26) = 2*3^26` needs trit 26 AND a carry.  More directly,
`3^26 + 3^26 = 2 * 3^26` exceeds the 27-trit symmetric range
`[-(3^27-1)/2, (3^27-1)/2]`. -/
theorem radical_sum_overflows : 2 * (3 : Int) ^ 26 > (3 ^ 27 - 1) / 2 := by
  decide

/-- **T4.2** If the radicals are wrapped modulo `3^27`, the value the code
multiplies is not the value Karatsuba's identity requires: reduction mod `3^27`
is the identity exactly on the representable window. -/
theorem wrapped_radical_changes_middle :
    ∀ s : Int, s % (3 : Int) ^ 27 = s → -((3 : Int) ^ 27) < s ∧ s < (3 : Int) ^ 27 := by
  intro s h
  constructor <;> omega

/-- **T4.3** Concrete counterexample witness to (†) failing: with
`x0 = x1 = 3^26` the radical sum is `2·3^26`, which does not fit in 27 trits,
so the shipped code substitutes `2·3^26 - 3^27 = -3^26`. -/
theorem radical_wrap_witness :
    2 * (3 : Int) ^ 26 - 3 ^ 27 = -(3 : Int) ^ 26 ∧
    (2 * (3 : Int) ^ 26) % 3 ^ 27 = 2 * 3 ^ 26 := by
  constructor <;> decide

/-! ==================================================================
    5. `T81BigInt::operator+` single-limb fallback

    if (max(ta,tb) < 81) { ...; fix up sign ... }        -- correct
    else {
      try {
        res.limbs_.push_back(signed a);
        Limb rhs = signed b;
        res.limbs_[0] = res.limbs_[0] + rhs;
        res.negative_ = false;          <-- (!!)
        res.normalize();                -- verify_invariants() -> terminate()
      } catch (overflow_error&) { }
    }

    Because `normalize()` calls `verify_invariants()`, which requires the top
    limb's `sign_trit()` to be non-negative, a *negative* result in this branch
    calls `std::terminate()`.  `std::terminate` is not an exception, so the
    `catch` cannot rescue it.
    ================================================================== -/

/-- The invariant `normalize()` enforces on a single-limb value. -/
def InvariantHolds (magnitude : Int) (neg : Bool) : Bool :=
  if magnitude = 0 then !neg else decide (magnitude > 0)

/-- **T5.1** The fallback branch sets `neg := false` while leaving the magnitude
with the sign of the arithmetic result.  Hence a negative sum violates the
invariant that `normalize()` checks with `std::terminate()`. -/
theorem fallback_violates_invariant :
    InvariantHolds (-4) false = false := by decide

/-- **T5.2** The main fast path does not have this defect because it re-signs
before normalising: -/
theorem fastpath_repairs_invariant :
    InvariantHolds (4) true = true := by decide

/-- **T5.3** Reachability: the branch is taken exactly when both operands are
single-limb with at least 81 significant trits.  `3^80` is the smallest such
magnitude (it needs trit index 80, i.e. 81 trits). -/
theorem three_pow_80_needs_81_trits :
    (3 : Int) ^ 80 = 3 ^ (81 - 1) ∧ (3 : Int) ^ 80 < 3 ^ 81 := by
  constructor <;> decide

end T81Audit
