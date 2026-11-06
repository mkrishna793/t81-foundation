# T81 audit — reproduction guide

Everything in this directory is self-contained and reproducible. Nothing here
is part of the shipped library; it is an audit harness.

See **[AUDIT_REPORT.md](AUDIT_REPORT.md)** for the findings.

## Requirements

| Tool | Version used | Needed for |
|---|---|---|
| `g++` | 13, C++20 | defect harness + probes |
| `python3` | 3.11 | oracle + Z3 driver |
| `z3-solver` | 5.3.3 (`python3 -m venv .venv && .venv/bin/pip install z3-solver`) | SMT layer |
| Node | ≥ 22 | Lean 4 runner (V8 export limit) |
| `lean4-wasm` | 0.0.0 (npm) | Lean 4 kernel + stdlib |

## 1. Defect harness (fastest path)

```bash
g++ -std=c++20 -O2 -Iinclude -o audit/build/findings audit/cpp/findings.cpp
T81_AXION_TRAP_STDERR=0 ./audit/build/findings
```

Prints `DEFECT` / `CLEAN` per check with the exact numbers. Several defects call
`std::terminate()`, so those checks are forked — the harness always exits
normally.

## 2. Exact-integer oracle

```bash
g++ -std=c++20 -O2 -Iinclude -o audit/build/limb_probe audit/cpp/limb_probe.cpp
./audit/build/limb_probe 400 987654321 > audit/logs/limb_probe.txt
python3 audit/smt/oracle.py < audit/logs/limb_probe.txt
```

The oracle decodes every emitted trit vector to ℤ from the *definition*
`Σ tᵢ·3ⁱ` and checks add / addc / mul_wide / booth / karatsuba / reference_mul /
subtraction against exact arithmetic. It flags both wrong values and
non-canonical digits.

## 3. SMT (Z3)

```bash
python3 audit/smt/z3_audit.py        # writes z3_results.json
```

Fourteen checks. `PROVED` = the universally-quantified claim is valid (UNSAT of
its negation); `CONFIRMED` = a concrete counterexample/model was found.

## 4. Lean 4

The sandbox allowlist blocks `release-assets.githubusercontent.com`, so
`elan`/`leanprover/lean4` release tarballs cannot be fetched. The real Lean 4
kernel is instead taken from the npm bundle:

```bash
mkdir -p /home/user/leanwasm && cd /home/user/leanwasm
npm init -y && npm install --no-save lean4-wasm node@24
cd node_modules/lean4-wasm && tar xzf lean-lib.tar.gz -C . && \
  mkdir -p lib && mv Init Init.olean Lake Lake.olean LakeMain.olean \
                       Lean Lean.olean Leanc.olean Std Std.olean lib/
```

`leanrun.js` reads `LEAN4_WASM_HOME` (default `/home/user/leanwasm`):

```bash
/home/user/leanwasm/node_modules/node/bin/node audit/lean/leanrun.js audit/lean/Stage1.lean
```

Expect ~2.5 minutes of fixed startup (the WASM build has no `--server` mode),
then `0 errors`. Batch statements per file — startup dominates. Node < 22 fails
with `exports count of 201110 exceeds internal limit of 100000`.

### What the Lean file proves

`Stage1.lean` contains 20 theorems, no `sorry`:
tryte codec correctness and non-injectivity; the carry chain's exact domain of
validity (`|s| ≤ 4`) with illegality witnesses outside it; the correctness of
floor-div-3 renormalisation; `T81Int` column-adder laws and the reachability of
overflow; the necessary-and-sufficient multiplication guard; the 27-trit radical
overflow that breaks Karatsuba; and the `T81BigInt` single-limb invariant
violation.

## 5. Algorithm measurements

The timing and scaling figures quoted in the report come from:

```bash
# per-operation cost at 48/54 trits
g++ -std=c++20 -O2 -Iinclude -o /tmp/bench -x c++ - <<'EOF'
… (see AUDIT_REPORT.md §7.1; the harness is in f_bench form)
EOF
```

and a `T81BigInt` scaling sweep at 64/128/256/512/1024 trits. Both are
single-threaded, 2 vCPU, gcc 13 `-O2`; treat the absolute numbers as indicative
and the *ratios* as the meaningful signal.
