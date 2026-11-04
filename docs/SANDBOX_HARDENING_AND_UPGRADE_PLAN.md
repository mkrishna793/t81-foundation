# Comprehensive Sandbox Hardening & Multi-Tier Upgrade Specification

**Document Version:** 1.0.0
**Target Architecture:** T81 Runtime, Memory Manager, Compiler / JIT, Axion Policy Engine, and Foreign Interchange.

---

## 1. Threat Model & Sandboxing Objectives

The T81 execution environment runs untrusted programs, autonomous agent policies, and user-supplied neural weights while guaranteeing:
1. **Memory Isolation:** No guest instruction may access host memory, execute writable heap, or bridge beyond its allocated segment.
2. **Compiler & JIT Integrity:** Dynamic code generation must enforce strict non-executable writes (W^X), prevent JIT-spraying, and preserve instruction pre-dispatch verification.
3. **Deterministic Execution Sandboxing:** Bounded resource execution (instructions, recursion, stack, reflection depth) to prevent DoS, CPU starvation, and non-deterministic timing channels.
4. **Capability-Governed Foreign Boundaries:** Zero unmediated host interaction; all file, network, and FFI operations must strictly transit through CanonFS content-addressed manifests and Axion capability tickets.

---

## 2. Memory Subsystem Hardening

### 2.1 Hardware-Enforced Guard Pages & Segmentation
- **Current State:** Segments (`Code`, `Stack`, `Heap`, `Tensor`, `Meta`) are contiguous logical offsets within a single contiguous `std::vector<int64_t>`.
- **Target Upgrade:**
  - Allocate discrete virtual memory regions per segment via `mmap(MAP_ANONYMOUS | MAP_PRIVATE)`.
  - Place PROT_NONE guard pages (minimum 4096 bytes / 1 host page) between every adjacent segment:
    `[GUARD (PROT_NONE)] [CODE] [GUARD] [STACK] [GUARD] [HEAP] [GUARD] [TENSOR] [GUARD] [META] [GUARD]`.
  - Any sequential buffer overflow, underflow, or wild pointer dereference immediately triggers hardware page faults (SIGSEGV) rather than silently corrupting adjacent guest segments.

### 2.2 W^X (Write XOR Execute) Protection
- **Target Upgrade:**
  - The `Code` segment must be mapped with `PROT_READ | PROT_EXEC` during execution.
  - All data segments (`Stack`, `Heap`, `Tensor`, `Meta`) must be strictly mapped `PROT_READ | PROT_WRITE` without `PROT_EXEC`.
  - For Tier 4 / reflection instructions (`WriteCode`), the runtime must temporarily toggle permissions via dual-mapped aliases (one writable unexecutable view for the engine, one executable non-writable view for the CPU), completely eliminating exploitable W+X pages.

### 2.3 Off-by-One and Integer Wraparound Remediation
- **Action Items:**
  1. Fix `vm/memory_segments.cpp` code segment check:
     Replace `a <= layout.code.limit` with `layout.code.contains(a)` (`a < limit`).
  2. Implement safe pointer arithmetic checking in `vm/vm.cpp`:
     ```cpp
     inline bool is_safe_add(uint64_t base, uint64_t offset, uint64_t limit) {
       return (base <= limit) && (offset <= limit - base);
     }
     ```
  3. Enforce stack underflow guard:
     Require `ctx.sp > ctx.stack_limit` and verify `ctx.stack_limit >= layout.stack.start`.

---

## 3. Compiler & JIT Subsystem Hardening

### 3.1 Sandboxed JIT Execution & Pre-Dispatch Invariants
- **Vulnerability:** Unchecked JIT compilation can bypass bytecode-level Axion pre-dispatch checks (`kStep`, `kMemLoad`, `kMemStore`).
- **Target Upgrade:**
  - **JIT Invariant Preservation:** Every JIT-compiled basic block must include inlined capability gas-check and Axion policy validation hooks at the block header.
  - **Memory Guard Inlining:** Memory access instructions in JIT code must emit hardware bounds checks (`cmp + jb / trap`) or rely on OS-level virtual memory fault handlers mapped to deterministic `Trap::BoundsFault`.
  - **Constant Blinding:** Implement JIT constant blinding for ternary/numeric constants to neutralize JIT spraying attacks against host instruction caches.

### 3.2 Formal Bytecode Verification before Compilation
- Before any trace or bytecode is submitted to the JIT compiler, run a single-pass type and bounds verifier (`BytecodeVerifier`):
  - Validates all register indices are strictly within $[0, 242]$.
  - Validates literal pool offsets and symbol table bounds.
  - Verifies basic block control flow graph acyclicity or bounded backward jumps.

---

## 4. Execution & Side-Channel Hardening

### 4.1 Fine-Grained Gas & Deterministic Instruction Budgets
- Enforce strict decremental execution budgets (`gas_remaining`):
  - Every opcode consumes a calibrated, deterministic cost (e.g., standard ALU = 1, Memory Load/Store = 3, Tensor Matmul = $M \times N \times K$, Reflection = 20).
  - When gas reaches 0, the VM halts deterministically with `Trap::ResourceExhausted`.
- Eliminates infinite loops, algorithmic complexity attacks, and CPU exhaustion.

### 4.2 Axion Bytecode Engine Hardening (`kernel/axion/policy_engine.cpp`)
- **Remediation:**
  - Add strict stream boundary validation for `read_u32()` and `read_u64()`:
    ```cpp
    auto read_u32 = [&]() -> std::optional<uint32_t> {
      if (pc + 4 > end) return std::nullopt;
      uint32_t v = static_cast<uint32_t>(*pc++);
      v |= (static_cast<uint32_t>(*pc++) << 8);
      v |= (static_cast<uint32_t>(*pc++) << 16);
      v |= (static_cast<uint32_t>(*pc++) << 24);
      return v;
    };
    ```
  - If a bytecode stream terminates prematurely, reject immediately with `VerdictKind::Deny`.

---

## 5. Host & I/O Capability Sandboxing (Foreign Boundary)

### 5.1 Linux Seccomp-BPF & System Call Confinement
- On Linux hosts, the worker execution threads running T81VM must be confined using `seccomp-bpf`:
  - **Allowed Syscalls:** `read`, `write` (to pre-opened stdout/stderr pipes), `futex`, `exit_group`, `sigreturn`.
  - **Denied Syscalls:** `execve`, `fork`, `socket`, `connect`, `open`, `openat` (unless mediated by CanonFS capability descriptors).
  - Any unauthorized host syscall attempt results in deterministic process termination.

### 5.2 Content-Addressed CanonFS Boundary
- All external tensor weights, neural checkpoints, and input datasets must be validated against their Base-81 content hashes before mounting into VM address space.
- Foreign filesystem paths must be strictly normalized; directory traversal (`../`) and symlink escapes are rejected at the policy validator boundary.

---

## 6. Implementation Roadmap

```
Phase 1: Immediate Flaw Remediation (Milestone 1)
  ├── Align memory segment limit checking (eliminate code segment off-by-one)
  ├── Add safe bounds checks in Axion policy bytecode reader
  └── Implement overflow-safe pointer addition helpers

Phase 2: Memory & Page Protection (Milestone 2)
  ├── Virtual memory allocator with hardware PROT_NONE guard pages
  ├── W^X memory page permissions on code vs data
  └── Dual-mapping architecture for Tier 4 reflection code writes

Phase 3: Formal Verification Pipeline Integration (Milestone 3)
  ├── Continuous Z3 SMT verification in CI for all arithmetic and codec modules
  └── Coq proof validation target in CMake/CTest for core ISA invariants

Phase 4: OS-Level Sandbox Confinement (Milestone 4)
  ├── Linux seccomp-bpf filter initialization on VM spawn
  ├── Strict content-hash verification on all external file descriptors
  └── Fine-grained deterministic execution gas accounting
```
