# T81 Foundation — Comprehensive Repository Research & Architecture Reassessment

**Document ID:** `docs/research/T81_FULL_REASSESSMENT_2026-10-25.md`
**Date:** October 25, 2026
**Status:** Complete / Final Research Assessment
**Author:** Independent Principal Research Engineer & Systems Auditor
**Target Repository:** T81 Foundation (`t81dev/t81-foundation`)
**Pinned Snapshot Baseline:** Commit `962fb5ef24b012f91a1a3d1cb18a12d2145e66f0` (Main Branch)
**Continuity Baseline:** Commit `860558b4ad1cf4160af30860b00b15c46165acd5`

---

## A. Executive Assessment

The T81 Foundation repository represents an **experimental scientific research framework and software engineering substrate** focused on two main research domains:

1. **Track R (Scientific Computing & Alternative Representations):** Investigating whether ternary arithmetic, balanced ternary representations, Base-81 digit grouping, SWAR vectorization, or 2-bit packed weight formats produce measurable or theoretically defensible computational, storage, or execution advantages over equivalent binary systems.
2. **Track F (Foundation Systems Engineering & Governance):** Building and evaluating deterministic virtual execution, content-addressed artifact storage (CanonFS), pre-side-effect policy authorization (Axion), immutable execution provenance, and detached accountable evidence verification (RFC-00D4).

### Key Reassessment Findings

- **Ternary CPU Arithmetic Advantage is Falsified & Frozen:** Software ternary arithmetic, Base-81 tryte grouping, and 2-bit packed ternary CPU matrix multiplication (MatVec) on general-purpose binary ALUs do **not** outperform standard binary integer or floating-point SIMD pipelines. While 2-bit weight packing saves 75% memory bandwidth, SIMD bit-shuffle decode instruction overhead limits execution speed to 75.3% of INT8 AVX2 in cache (`experiments/packed_ternary_matvec/`). Software CPU ternary optimization is formally **frozen (STOP status)**. 2-bit packing is retained strictly as an efficient on-disk weight storage and interchange format.
- **Accountable Computation is a Protocol, Not a VM:** Comparative experiments (`examples/governance/accountable-computation/` and `experiments/research_001/`) demonstrate that pre-side-effect policy enforcement and offline auditability do not inherently require a custom virtual machine (T81VM/TISC). A conventional WebAssembly runtime (Wasmtime) with host FFI capability boundaries and detached canonical evidence bundles (RFC-00D4) achieves equal 0.0% policy breach rates and 100% offline auditability with lower system complexity.
- **Deterministic AI Execution & Provenance is the Primary Validated Capability:** T81's strongest, most defensible contribution is its bit-exact cross-platform determinism engine (DCP), soft-float transcendentals (RFC-0030), content-addressed artifact file system (CanonFS / RFC-00D1), and fail-closed policy authorization engine (Axion).
- **Architectural Framing Shift:** The project is correctly understood as a hypothesis-driven empirical research experiment, not a commercial desktop operating system or a native ternary hardware platform.

---

## B. Architecture Inventory

A comprehensive audit of the primary subsystems in the repository yields the following implementation, testing, and operational status:

| Subsystem | Implemented Role | Key Source Paths | Test & Verification Coverage | Implementation Status |
| :--- | :--- | :--- | :--- | :--- |
| **TISC ISA (v1.9.0)** | 81-opcode instruction set architecture for T81 execution | `include/t81/isa/opcodes.hpp`, `src/isa/` | `tests/cpp/test_isa.cpp`, CTest #391–396 | **Frozen (v1.9.0)** |
| **T81VM Interpreter** | Virtual machine executing TISC bytecode with DCP determinism | `src/vm/vm.cpp`, `include/t81/vm/vm.hpp` | `tests/cpp/vm_determinism_property_test.cpp` | **Beta Runtime** |
| **Axion Policy Kernel** | Pre-dispatch policy evaluation engine (`eval_axion_call`) | `src/axion/`, `include/t81/kernel/axion/` | `tests/cpp/axion_agent_invoke_policy_test.cpp`, CTest #154 | **Alpha Governance** |
| **CanonFS** | Content-addressed storage and foreign filesystem interchange (RFC-00D1) | `src/core/fs/`, `include/t81/fs/` | `tests/cpp/canonfs_test.cpp`, CTest #84–85 | **Beta Identity FS** |
| **T81Lang Tooling** | Frontend lexer, parser, AST, IR generator, and bytecode compiler | `src/lang/`, `include/t81/lang/` | `tests/cpp/test_lang.cpp`, `t81 code check/build` | **Beta Language** |
| **Model Intake Bridge** | GGUF/Safetensors model header inspection & tensor inventory | `src/tools/model/`, `tools/cli/ai/` | `t81_weights_load_test` (CTest #166) | **Metadata Bridge** |
| **Determinism Facilities** | Soft-float math (RFC-0030 CORDIC), bit-exact containers, CanonHash-81 | `src/core/isa/math/`, `vm/determinism/` | CTest #355–362 | **Functional / DCP** |
| **Ternary / Base-81 Codecs** | SWAR SIMD emulation, Base-81 tryte packing, 2-bit codecs | `src/core/codec/`, `src/isa/math/quantization/` | `tests/cpp/test_swar.cpp`, `test_base81_simd.cpp` | **Experimental / Frozen** |
| **Accountable Evidence (RFC-00D4)** | Detached evidence bundle generator & verifier | `examples/governance/accountable-computation/` | `t81_accountable_experiment` (CTest #4) | **Research Reference** |

### External Dependencies
- **Header-Only / Vendored Dependencies:** Asio, nlohmann/json, FTXUI (retrieved via CMake `FetchContent`), OpenSSL (optional for Ed25519 signing).
- **Toolchain Requirements:** C++23 compliant compiler (GCC 13+, Apple Clang 16+, MSVC 2022+), CMake 3.25+, Ninja, Python 3.10+.

---

## C. Historical Lineage & Hanoi Research Inheritance

The historical progression of the T81 computational concepts spans four distinct phases:

```
[ Knuth Seminumerical Algorithms ]
 (Balanced ternary, positional number systems, 3-valued logic)
               |
               v
[ Hanoi CWEB Exploration ]
 (CWEB implementation, 41 historical source blobs, experimental arithmetic)
               |
               v
[ T81Lang / TISC ISA / T81VM Engine ]
 (TISC v1.9.0 opcode frozen specification, C++ interpreter, SWAR ternary vectorization)
               |
               v
[ Axion Policy OS & CanonFS Core ]
 (Fail-closed pre-dispatch governance, content-addressed storage, decision bundles)
               |
               v
[ T81 Foundation Stack ]
 (Deterministic execution, policy-gated AI runtimes, accountable computation framework)
```

### Archaeological Findings

1. **Hanoi CWEB Inheritance:** Code in `experimental/hanoi/` preserves the CWEB-based exploration of ternary state machines. Historical audits identified 41 shared source blobs transferred from Hanoi.
2. **Arithmetic Defect Correction:** Early Hanoi implementations suffered from carry-propagation boundary errors and ambiguous zero representations in signed-digit representations. These were mathematically analyzed and corrected during TISC ISA stabilization (`docs/research/TERNARY_ARITHMETIC_AND_RADIX_COMPARISON.md`).
3. **Transformation of Scope:** The research shifted from attempting to build a general-purpose desktop operating system or native ternary hardware CPU toward evaluating **deterministic, policy-governed runtime boundaries for AI artifacts and accountable evidence**.

---

## D. Hypothesis Assessment (H01–H08 Register)

The project evaluates eight central hypotheses under its research charter:

### H01: Ternary ALU & Software Arithmetic Advantage
- **Motivation:** Explore whether 3-valued balanced ternary logic provides higher arithmetic density or faster addition.
- **Formal Claim:** Fixed-width Base-81 software tryte addition on binary ALUs outperforms 64-bit binary ALU integer addition.
- **Implementation Status:** Tested in `experiments/radix_comparison/` (`t81_radix_comparison_test`).
- **Supporting / Contradictory Evidence:** Grouping 4 trits into Base-81 trytes reduced software addition latency from $105.3\text{ ns}$ down to $11.7\text{ ns}$. However, binary 64-bit ALU addition takes $1.1\text{ ns}$ ($10.6\times$ faster).
- **Binary Baseline:** 64-bit binary integer addition on native CPU ALU.
- **Assumptions & Limits:** Emulated software ternary on binary ALUs incurs masking/shifting overhead.
- **Confidence Level:** High ($100\%$ falsified for software CPU arithmetic).
- **Decision / Status:** **FALSIFIED / FROZEN**.

### H02: SWAR & Base-81 SIMD Vectorization Advantage
- **Motivation:** Test SWAR (Simd Within A Register) for parallel ternary operations (`TNOT_SWAR`, `TAND_SWAR`, `TOR_SWAR`).
- **Formal Claim:** SWAR ternary vector operations provide higher compute throughput on binary vector units than packed binary SIMD.
- **Implementation Status:** Implemented in `include/t81/codec/base81_packed.hpp` and `src/core/codec/base81_packed.cpp`.
- **Supporting / Contradictory Evidence:** SWAR allows parallel tryte manipulation, but digit decoding and bit-shuffling tax offsets parallel ALU gains on general binary hardware.
- **Binary Baseline:** AVX2 / NEON 8-bit / 16-bit binary SIMD vector operations.
- **Confidence Level:** High.
- **Decision / Status:** **FALSIFIED / FROZEN**.

### H03: Packed 2-Bit Ternary Model Inference / MatVec Advantage
- **Motivation:** Evaluate 1.58-bit / ternary neural network weight matrix-vector multiplication ($W \in \{-1, 0, 1\}$).
- **Formal Claim:** On-the-fly 2-bit packed ternary weight decoding accelerates CPU MatVec throughput beyond INT8 AVX2/NEON.
- **Implementation Status:** Implemented in `experiments/packed_ternary_matvec/` (`t81_packed_ternary_matvec_oracle_test`).
- **Supporting / Contradictory Evidence:** VPSHUFB nibble lookup table decoding achieves $12.91\text{ GFLOP/s}$ on $4096^2$ matrices, saving $75\%$ DRAM bandwidth. However, execution speed reaches only $75.3\%$ of INT8 AVX2 ($17.15\text{ GFLOP/s}$) due to SIMD bit-shuffle decode instruction tax in L1/L2 cache.
- **Binary Baseline:** INT8 AVX2 / NEON vector dot product (`vpdpbusd` / `vdot`).
- **Confidence Level:** High ($100\%$ verified).
- **Decision / Status:** **FALSIFIED for execution speed; REVISED for weight storage format**.

### H04: Custom ISA/VM Necessity for Accountable Computation
- **Motivation:** Determine whether policy-gated execution requires a custom ISA and VM.
- **Formal Claim:** A bespoke VM (T81VM/TISC) is required to guarantee pre-side-effect policy evaluation and evidence generation.
- **Implementation Status:** Tested in `examples/governance/accountable-computation/` and `experiments/research_001/`.
- **Supporting / Contradictory Evidence:** Wasmtime + OPA host capability baseline (Arm iii / Baseline-X) achieved identical $0.0\%$ breach rates under mid-flight policy revocation and $100\%$ detached evidence auditability with $10\times$ lower host C++ LOC.
- **Binary Baseline:** Wasmtime + Open Policy Agent + RFC-00D4 Evidence Schema.
- **Confidence Level:** High.
- **Decision / Status:** **REVISED / UNFOUNDED as a necessity (Accountable computation is a Protocol, Not a Runtime)**.

### H05: Inner-Dispatch Policy Governance Coupling
- **Motivation:** Test whether policy evaluation must occur inside the VM inner execution loop.
- **Formal Claim:** `eval_axion_call` inside `AgentInvoke` / `WLoad` dispatch prevents unauthorized side effects before state mutation.
- **Implementation Status:** Implemented in `src/axion/` and `src/vm/vm.cpp` (`t81_vm_predispatch_policy_deny_logging_test`).
- **Supporting / Contradictory Evidence:** In-kernel pre-dispatch policy evaluation strictly prevents side effects ($0.0\%$ breach rate). However, host FFI boundaries in Wasm provide equivalent fail-closed isolation.
- **Binary Baseline:** Host FFI import boundary / API gateway pre-commit hook.
- **Confidence Level:** High.
- **Decision / Status:** **ESTABLISHED for in-VM execution; REVISED for universal architecture**.

### H06: Monolithic Stack vs. Detached Protocol Evidence
- **Motivation:** Evaluate whether accountable computation requires a vertically integrated stack (TISC + Axion + CanonFS).
- **Formal Claim:** Auditability requires the complete monolithic T81 stack at both creation and verification time.
- **Implementation Status:** Evaluated via RFC-00D4 evidence schemas and `verify.py`.
- **Supporting / Contradictory Evidence:** Detached JSON evidence bundles (RFC-00D4) verified via standalone Python/CLI tools achieve $100\%$ offline auditability across heterogeneous hosts without running T81VM.
- **Binary Baseline:** Standalone CLI verification tools using RFC 8785 canonical JSON and Ed25519 signatures.
- **Confidence Level:** High.
- **Decision / Status:** **REVISED (Detached evidence schemas are superior for system interoperability)**.

### H07: Bit-Exact Cross-Platform Execution Determinism (DCP)
- **Motivation:** Provide bit-identical state machine replication across x86_64, ARM64, and WebAssembly.
- **Formal Claim:** T81VM with soft-float transcendentals (RFC-0030 CORDIC/Minimax) guarantees bit-exact cross-platform execution.
- **Implementation Status:** Implemented in `src/core/isa/math/t81_soft_math/` and `tests/determinism/`.
- **Supporting / Contradictory Evidence:** $100\%$ pass rate on cross-platform property-based determinism tests (`tests/cpp/vm_determinism_property_test.cpp`).
- **Binary Baseline:** Host C/C++ `libm` IEEE 754 default floating-point execution.
- **Confidence Level:** High.
- **Decision / Status:** **ESTABLISHED for Deterministic Core Profile (DCP)**.

### H08: Content-Addressed Artifact Identity & Immutable Provenance (CanonFS)
- **Motivation:** Provide cryptographic content addressing for programs, tensors, policies, and evidence.
- **Formal Claim:** Content-addressed storage (CanonFS / RFC-00D1) delivers immutable artifact identity and foreign filesystem interchange.
- **Implementation Status:** Implemented in `src/core/fs/` (`canonfs_identity_contract_test`, `canonfs_policy_boundary_test`).
- **Supporting / Contradictory Evidence:** CanonFS import/export and CanonHash-81 hashing successfully verify byte integrity across platforms.
- **Binary Baseline:** Path-based filesystems and standard SHA-256 integrity metadata.
- **Confidence Level:** High.
- **Decision / Status:** **ESTABLISHED for byte identity and storage integrity**.

---

## E. Claims Audit

An audit of claims in former documentation against implemented code and test evidence reveals:

| Category | Claim Summary | Empirical Status | Source Evidence | Reassessment Finding |
| :--- | :--- | :--- | :--- | :--- |
| **ISA / Runtime** | "TISC v1.9.0 is a frozen, deterministic instruction set." | **SUPPORTED** | `include/t81/isa/opcodes.hpp`, CTest #391–396 | Core 81 opcodes and bit-exact execution are fully implemented and tested. |
| **Policy Governance** | "Axion policy checks prevent unauthorized side effects before dispatch." | **SUPPORTED** | `src/axion/`, CTest #154 | Pre-dispatch `eval_axion_call` fails closed and logs policy denials before state mutation. |
| **Artifact Storage** | "CanonFS provides content-addressed artifact storage and interchange." | **SUPPORTED** | `src/core/fs/`, CTest #84–85 | CanonFS import, export, and foreign interchange (RFC-00D1) function as specified. |
| **Deterministic Soft-Float** | "RFC-0030 soft-float transcendentals provide cross-platform parity." | **SUPPORTED** | `src/core/isa/math/`, CTest #355 | Soft-float CORDIC math provides bit-exact parity across x86_64 and ARM64. |
| **Ternary CPU Speedup** | "Software Base-81 / SWAR ternary execution accelerates computation on binary CPUs." | **CONTRADICTED** | `experiments/radix_comparison/` | Software tryte addition is $10.6\times$ slower than native 64-bit binary ALU addition. |
| **MatVec Inference** | "2-bit packed ternary MatVec executes faster than INT8 AVX2." | **CONTRADICTED** | `experiments/packed_ternary_matvec/` | VPSHUFB 2-bit MatVec reaches $75.3\%$ of INT8 AVX2 speed due to SIMD decode instruction tax. |
| **Custom VM Requirement** | "Accountable computation requires a custom VM and kernel." | **UNSUPPORTED** | `examples/governance/accountable-computation/` | Wasmtime + OPA + RFC-00D4 evidence schema provides equivalent policy and audit guarantees. |
| **Hardware Platform** | "T81 runs on physical ternary microprocessors." | **UNSUPPORTED** | Repository inspection | No physical ternary hardware exists in the codebase; all execution uses binary hardware emulation. |

---

## F. Technical Debt and Risks

1. **Process Security & Shell Traversal:**
   - Command execution in CLI tools should continue avoiding `std::system` and shell expansion to prevent command injection risks.
   - External processes must strictly use `t81::support::run_process` (`include/t81/support/process.hpp`) with explicit argument vectors.
2. **Windows PATH Parsing:**
   - PATH environment variable entries on Windows may contain surrounding quotes (e.g. `"C:\Program Files"`), requiring string trimming before passing to `std::filesystem::path`.
3. **Durable Witness Isolation:**
   - Security audits (`experiments/durable_witness/`) established that an isolated SQLite WAL witness store remains vulnerable to snapshot rollback without an external monotonic checkpoint or log. HMAC receipts must sign all metadata fields (`witness_id`, `payload_hash`, `total_amount`, `timestamp`).
4. **Build Warnings:**
   - Apple Clang compiler warnings regarding dangling references to temporaries in `examples/research/validated-descriptors/runner.cpp` line 144 should be cleaned up during maintenance.

---

## G. Research Opportunities

1. **Galois Field $\text{GF}(3^9)$ vs $\text{GF}(2^8)$ Storage & Parity:** Evaluating Base-81 Galois Field arithmetic ($\text{GF}(3^9)$) for Reed-Solomon storage parity in CanonFS versus hardware-accelerated binary Galois Field instructions (`GF2P8AFFINEINVQB`).
2. **Multi-Precision Signed-Digit (RSD) Crossover:** Microbenchmarking Avizienis carry-free addition across 128-bit to 512-bit integer widths to identify the exact crossover point where carry-free digit addition overcomes digit-unpacking overhead on binary SIMD vector units.
3. **Deterministic AI Execution & Trace Provenance:** Validating soft-float transcendentals (RFC-0030) and trace hashing across large neural network inference execution graphs.

---

## H. Next Discriminating Experiment

### Title
**Cross-Platform Bit-Identical Deterministic AI Execution & Trace Provenance Parity (Candidate 3)**

### Objective
Determine whether T81's Deterministic Math Subsystem (RFC-0030 soft-float transcendentals) and Canonical Trace Cache (RFC-0028) can achieve **100% bit-identical execution trace parity** across x86_64, ARM64, and WebAssembly targets under Axion policy governance, while incurring a **$<5\%$ runtime performance tax** relative to non-deterministic host `libm` floating-point execution.

### Experimental Design
1. **Target Subsystems:** `tests/determinism/` and `vm/determinism/`.
2. **Evaluation Workload:** Softmax, LayerNorm, and multi-layer MLP token generation execution traces across $100,000$ adversarial floating-point inputs (including subnormals, NaNs, infinities, and extreme exponents).
3. **Comparison Baseline:** Host IEEE 754 floating-point `libm` execution on x86_64, ARM64, and WebAssembly.
4. **Independent Oracle:** Bitwise hash comparison of execution traces using `CanonHash81`.
5. **Acceptance Criteria:**
   - Bitwise trace equality: `CanonHash81(Trace_x86_64) == CanonHash81(Trace_ARM64) == CanonHash81(Trace_WASM)`.
   - Execution latency tax: $\frac{\text{Latency}_{\text{DeterministicSoftFloat}} - \text{Latency}_{\text{HostLibm}}}{\text{Latency}_{\text{HostLibm}}} \times 100\% < 5.0\%$.
6. **Stopping Conditions:**
   - If bitwise trace equality fails on any input, halt and record numeric drift bug.
   - If execution tax exceeds $20.0\%$, trigger lookup-table soft-float optimization.

---

## I. README Change Rationale

The root `README.md` was rewritten to align project documentation with empirical source code evidence:

1. **Clear Project Identity:** Replaced vague architectural promises with a precise description of T81 Foundation as an experimental research program investigating deterministic execution, policy governance, provenance, and alternative computational representations.
2. **Honest Hypothesis Ledger:** Included explicit results for H01–H08, openly disclosing negative experimental findings (such as software ternary CPU arithmetic speedup being falsified).
3. **Reproducible Instructions:** Provided verified, exact CMake, Ninja, CTest, and Python regression commands that execute cleanly in the codebase today.
4. **Removed Promotional Badges & Claims:** Removed unsupported performance badges, claims of physical ternary hardware, and assertions that custom VMs are strictly necessary for accountable evidence.

---

## J. Evidence Ledger

- **Main Commit SHA:** `962fb5ef24b012f91a1a3d1cb18a12d2145e66f0`
- **Continuity Commit SHA:** `860558b4ad1cf4160af30860b00b15c46165acd5`
- **Verification Commands Executed:**
  - `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DT81_BUILD_TESTS=ON`
  - `ninja -C build t81 t81_radix_comparison t81_packed_ternary_matvec`
  - `python3 scripts/ci/run_research_regressions.py --build-dir build` (Result: 8/8 tests passed in 4.68s)
- **Primary Source Files Examined:**
  - `include/t81/isa/opcodes.hpp`
  - `src/vm/vm.cpp`
  - `src/axion/`
  - `src/core/fs/`
  - `experiments/radix_comparison/`
  - `experiments/packed_ternary_matvec/`
  - `examples/governance/accountable-computation/`
  - `docs/research/TERNARY_RESEARCH_CLOSURE_AND_NEXT_HYPOTHESIS.md`
  - `docs/research/consolidation/README.md`
