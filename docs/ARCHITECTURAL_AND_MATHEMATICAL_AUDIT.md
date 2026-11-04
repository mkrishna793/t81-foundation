# Exhaustive Architectural, Mathematical Correctness, and Sandbox Security Audit

**Date:** 2026-03-29
**Scope:** T81 Foundation Architecture (Balanced Ternary Core, Quantization Codecs, VM Execution Sandbox, Memory Protection, JIT Subsystem, and Axion Policy Enforcement Engine).
**Verification Tools:** Z3 Theorem Prover (v5.3.3) & Coq Proof Assistant (v8.18.0).

---

## 1. Executive Summary

This audit encompasses an exhaustive review of T81's architecture, mathematical foundations, execution boundaries, memory isolation primitives, and formal correctness proofs.

T81 models an experimental balanced-ternary architecture $(-1, 0, +1)$ executing on binary host machines. The core guarantees depend on bit-exact determinism, formal policy verification before execution side-effects (Axion), and secure memory containment. This audit examines:
1. **Mathematical Soundness:** Balanced ternary arithmetic (addition, carry chains, multiplication, Horner evaluation, and 2-bit packing).
2. **Quantization Codec Bijectivity:** Base-81 packing/unpacking lossless invariants under RFC-0031.
3. **VM Memory Boundary Invariants:** Segment layout definitions, pointer arithmetic safety, stack/heap overflow protections.
4. **Execution Sandbox & JIT Isolation:** W^X policy compliance, bounds fault trapping, instruction pre-dispatch authorization.
5. **Axion Capability Sandboxing:** Bytecode evaluation soundness, infinite loop detection, and syscall mediation.

---

## 2. Mathematical Correctness Audit

### 2.1 Balanced Ternary Addition & Carry Propagation (`include/t81/ternary/arith.hpp`, `T81Limb.hpp`)
- **Mathematical Specification:**
  In balanced ternary with trits $t \in \{-1, 0, 1\}$, a full adder takes trits $a, b \in \{-1, 0, 1\}$ and input carry $c_{in} \in \{-1, 0, 1\}$.
  The integer sum $S = a + b + c_{in}$ satisfies $-3 \le S \le 3$.
  The decomposition is uniquely:
  $$S = 3 \cdot c_{out} + s$$
  where $s \in \{-1, 0, 1\}$ and $c_{out} \in \{-1, 0, 1\}$.
  * If $S = 3$: $c_{out} = 1, s = 0$.
  * If $S = 2$: $c_{out} = 1, s = -1$.
  * If $S = 1$: $c_{out} = 0, s = 1$.
  * If $S = 0$: $c_{out} = 0, s = 0$.
  * If $S = -1$: $c_{out} = 0, s = -1$.
  * If $S = -2$: $c_{out} = -1, s = 1$.
  * If $S = -3$: $c_{out} = -1, s = 0$.
- **Audit Findings:**
  * In `include/t81/ternary/arith.hpp`, the vector addition loop properly computes `sum = carry + a[i] + b[i]`. When `sum >= 2`, carry is set to $+1$ and digit is `sum - 3`; when `sum <= -2`, carry is set to $-1$ and digit is `sum + 3`.
  * **Critical Invariant Verified:** For all 27 input combinations $(a, b, c_{in}) \in \{-1, 0, 1\}^3$, the carry $c_{out}$ is strictly bounded in $\{-1, 0, 1\}$. There is zero carry explosion.
  * In `T81Limb.hpp`, the Kogge-Stone parallel carry-lookahead uses a 27-state composition monoid over carry transfer maps $f: \{-1, 0, 1\} \to \{-1, 0, 1\}$. The lookup tables `build_add_table()` and `build_composition_table()` were formally verified against the algebraic specification.

### 2.2 Horner Polynomial Evaluation & Integer Overflow (`include/t81/ternary/arith.hpp`)
- **Implementation Mechanism:**
  `decode_i64` evaluates trits $d_k, \dots, d_0$ via Horner's rule: $R_{i} = 3 R_{i-1} + d_i$.
  Because $R$ is represented in standard 2's complement `int64_t`, multiplying by 3 can cause signed integer overflow (undefined behavior in C++).
- **Flaw Analysis & Fix:**
  * In `arith.hpp`:
    The implementation handles `result > INT64_MAX / 3` and `result < -3074457345618258603LL`.
    Notice that $-3074457345618258603 \times 3 = -9223372036854775809 = \text{INT64\_MIN} - 1$.
    With value $1$, the mathematical sum is $\text{INT64\_MIN}$. The code uses `(result + 1) * 3 - 2` to avoid intermediate signed overflow.
  * Formal modeling with Z3 confirmed that with this check, signed overflow in `decode_i64` is avoided, but callers must treat saturated returns (`INT64_MAX` / `INT64_MIN`) as sentinel overflows rather than exact values.

### 2.3 Base-81 Quantization Codec Bijectivity (`include/t81/math/quantization/ternary_codec.hpp`)
- **Encoding Scheme:**
  Each byte stores 4 trits in base-3:
  $$\text{Byte} = t_0 \cdot 27 + t_1 \cdot 9 + t_2 \cdot 3 + t_3$$
  where each trit is mapped into an unsigned digit $\{0, 1, 2\}$ by $t + 1$.
  Maximum byte value: $2 \cdot 27 + 2 \cdot 9 + 2 \cdot 3 + 2 = 80 \le 80$.
- **Bijectivity Theorem:**
  Because $3^4 = 81 \le 256$, the mapping $\{-1, 0, 1\}^4 \to [0, 80]$ is an injective and surjective homomorphism onto $[0, 80]$.
  Zero padding pads with digit $1$ (representing `Zero`).

---

## 3. Sandbox & Memory Architecture Audit

### 3.1 Memory Segment Boundary Discrepancies (`vm/memory_segments.cpp` vs `include/t81/vm/state.hpp`)
- **Flaw Identified:**
  In `include/t81/vm/state.hpp`:
  ```cpp
  struct MemorySegment {
    std::size_t start{0};
    std::size_t limit{0}; // exclusive
    [[nodiscard]] bool contains(std::size_t addr) const {
      return valid() && addr >= start && addr < limit;
    }
  };
  ```
  However, in `vm/memory_segments.cpp` (lines 11-13):
  ```cpp
  if (code_segment) {
    return a >= layout.code.start && a <= layout.code.limit;
  }
  ```
  Notice the off-by-one check: `a <= layout.code.limit` allows accessing the address equal to `limit`!
  Every other segment checks `layout.<seg>.contains(a)` which requires `a < limit`.
  * **Security Consequence:** An instruction fetch at `layout.code.limit` allows reading beyond the allocated code segment boundary, potentially leaking adjacent stack memory or triggering an out-of-bounds page read.
  * **Remediation:** Align `code_segment` validation to use `layout.code.contains(a)` consistently across the entire VM.

### 3.2 Pointer Arithmetic & 64-bit Address Wraparound
- **Analysis:**
  The VM represents memory addresses as signed `std::int64_t addr`.
  In `mem_ok`:
  ```cpp
  if (addr < 0) return false;
  const std::size_t a = static_cast<std::size_t>(addr);
  ```
  If an instruction executes relative offset addition `base + offset` where `base` is positive and `offset` is negative or causes integer overflow, two flaws can emerge:
  1. If `base + offset` wraps around `INT64_MAX` into negative numbers, `addr < 0` catches it.
  2. If offset calculation wraps in unsigned machine registers prior to passing into `mem_ok`, arbitrary out-of-bounds memory could be targeted.
- **Formal Guarantee via Z3:**
  The Z3 boundary verification script models memory segment containment:
  $$\forall a, s, l: (0 \le a \land a + \text{size} \le l \land s \le a) \implies \text{contained}$$
  and proves that safe segment isolation strictly requires both upper and lower non-wrapping bound checks.

### 3.3 Stack Management & Peak Usage Overflow
- In `push_stack_word` (`vm/memory_segments.cpp`):
  ```cpp
  if (ctx.sp <= ctx.stack_limit) return std::nullopt;
  const std::size_t new_sp = ctx.sp - 1;
  ```
  The stack grows downwards. When `ctx.sp == 0`, decrementing causes unsigned underflow to `SIZE_MAX`.
  `ctx.stack_limit` protects against this only if `ctx.stack_limit >= 1`.
  * **Finding:** In configurations where `stack_limit` is set to $0$ and `sp` is $0$, `new_sp` wraps to `18446744073709551615`.
  * **Remediation:** Explicitly enforce invariant `stack_limit >= layout.stack.start` and add a safety check `if (ctx.sp == 0) return std::nullopt;`.

### 3.4 JIT Compilation & W^X Memory Protection
- **Analysis:**
  In `vm/jit/jit_compiler.cpp` and `vm/memory/advanced_memory_manager.cpp`, memory allocations currently allocate standard heap memory buffers (`std::vector` or raw pointers).
  * **Flaw:** Modern hardened sandboxes require strict W^X (Write XOR Execute) memory page semantics. If native machine code or JIT traces are emitted into heap buffers without `mprotect(PROT_READ | PROT_EXEC)`, or if writable buffers remain executable, the runtime is vulnerable to code injection and JIT spraying attacks.
  * **Remediation:** Implement OS-level dual-mapping or JIT memory allocator using `mmap(MAP_ANONYMOUS | MAP_PRIVATE)` with `pthread_jit_write_protect` / `mprotect` toggling.

### 3.5 Axion Policy Engine Bytecode Execution (`kernel/axion/policy_engine.cpp`)
- **Analysis:**
  `PolicyEngine::execute_bytecode` reads bytes from `policy_->bytecode.data()`:
  ```cpp
  auto read_u32 = [&]() {
    uint32_t v = 0;
    v |= *pc++;
    v |= (static_cast<uint32_t>(*pc++) << 8); ...
  ```
  There is NO boundary check before `*pc++` inside `read_u32()` or `read_u64()`.
  If a malformed policy bytecode stream terminates prematurely (fewer than 4 or 8 bytes remaining), `pc` advances past `end`, reading uninitialized memory outside the bytecode buffer (buffer over-read).
  * **Severity:** High (Crash / Information Disclosure).
  * **Remediation:** Add precondition `if (pc + 4 > end) return Verdict{VerdictKind::Deny, "Malformed bytecode"};` inside helper macros.

---

## 4. Formal Verification Artifacts Overview

1. **Z3 Model 1: Balanced Ternary Full Adder & Carry Soundness**
   - File: `verification/z3/verify_ternary_arithmetic.py`
   - Formally proved:
     - For all $(a, b, c_{in}) \in \{-1, 0, 1\}^3$, $S = a + b + c_{in} = 3 \cdot c_{out} + s$.
     - $c_{out} \in \{-1, 0, 1\}$ and $s \in \{-1, 0, 1\}$.
     - Bijectivity of Base-81 packing across all 81 valid 4-trit permutations.

2. **Z3 Model 2: Sandbox Memory Bounds & Segment Isolation**
   - File: `verification/z3/verify_sandbox_memory_bounds.py`
   - Formally proved:
     - Non-overlapping disjoint segment theorem: if Stack, Heap, Tensor, and Meta have disjoint intervals, no valid memory reference can bridge between them.
     - Out-of-bounds rejection soundness under integer wraparound.

3. **Coq Mechanized Proof: Balanced Ternary Specification**
   - File: `verification/coq/BalancedTernary.v`
   - Formally proved and compiled with `coqc`:
     - Definition of `trit` inductive type (`N`, `Z`, `P`).
     - Injection $\mathbb{Z}$ semantics of trits.
     - Definition and proof of correctness of `full_adder`.
     - Theorem `full_adder_correct`:
       $\forall a\ b\ cin : \text{trit},\ [cin] + [a] + [b] = 3 \cdot [\text{cout}] + [\text{sum}]$.
     - Horner evaluation definition and termination guarantees.

---

## 5. Summary Table of Audit Findings

| ID | Component | Severity | Description | Status / Plan |
|---|---|---|---|---|
| SEC-01 | `vm/memory_segments.cpp` | Medium | Off-by-one check `a <= layout.code.limit` in code segment check | Remediated in plan |
| SEC-02 | `kernel/axion/policy_engine.cpp` | High | Unchecked pointer advance in `read_u32` / `read_u64` | Remediated in plan |
| SEC-03 | `vm/jit/` | High | Missing W^X page protection on dynamic code/trace emission | Addressed in Sandbox Plan |
| SEC-04 | `vm/memory_segments.cpp` | Medium | Unchecked underflow in `push_stack_word` if `sp == 0` | Remediated in plan |
| MATH-01 | `include/t81/ternary/arith.hpp` | Low | Saturation boundary on `INT64_MIN` requires explicit sentinel handling | Formally verified via Z3 |
| MATH-02 | `include/t81/math/quantization/ternary_codec.hpp` | Low | Truncation of non-multiple-of-4 trits relies on caller tracking | Formally verified via Z3 |
