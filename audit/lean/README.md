# Lean 4 layer

`Stage1.lean` contains 20 theorems about the **shipped** T81 algorithms, all
checked by the Lean 4 kernel: **0 errors, 0 `sorry`**.

## Why not `elan`

The audit sandbox allowlists only `github.com`, `codeload.github.com`,
`api.github.com`, `registry.npmjs.org`, `pypi.org`, `files.pythonhosted.org`.
Lean's release assets live on `release-assets.githubusercontent.com`, which is
not reachable (`SSL_ERROR_SYSCALL`), and `api.github.com`'s asset endpoint only
302-redirects there. No PyPI package ships a Lean toolchain (the `lean4` and
`elan` names are unrelated projects).

So the real Lean 4 kernel is taken from the npm-distributed `lean4-wasm`
bundle: Lean 4 compiled to WebAssembly together with its full standard library
(4,196 `.olean` files).

## Provisioning

```bash
mkdir -p /home/user/leanwasm && cd /home/user/leanwasm
npm init -y
npm install --no-save lean4-wasm node@24      # node >= 22 required
cd node_modules/lean4-wasm
tar xzf lean-lib.tar.gz -C .
mkdir -p lib
mv Init Init.olean Lake Lake.olean LakeMain.olean Lean Lean.olean Leanc.olean \
   Std Std.olean lib/
```

Node **must** be ≥ 22: earlier V8 refuses the bundle with

```
CompileError: WebAssembly.instantiate(): exports count of 201110
exceeds internal limit of 100000
```

## Running

```bash
LEAN4_WASM_HOME=/home/user/leanwasm \
  /home/user/leanwasm/node_modules/node/bin/node \
  audit/lean/leanrun.js audit/lean/Stage1.lean
```

`leanrun.js` sets `LEAN_PATH` to the extracted `lib/` and lets the Emscripten
glue mount the host filesystem, so no file copying into the wasm FS is needed.

Expect **~2.5 minutes of fixed startup** (this build has no `--server`/`--worker`
mode, so the whole stdlib is re-read per process), then `0 errors`. Batch
statements per file; do not run one theorem per process.

## Contents

| Group | Theorems |
|---|---|
| Tryte codec (`packing.hpp`) | `decode_emits_legal_trits` (exhaustive over 27), `decode_is_base3`, `encode_decode_id`, **`enc_not_injective`**, `enc_legal_trits_in_range` |
| Carry chain (`reference_mul`, `booth_mul_trits`) | `shipStep_preserves_value`, `shipStep_legal_of_small`, **`shipStep_illegal_outside_window`**, `shipped_requires_bound` |
| `T81Int<N>` | `addDigit_legal`, **`add_can_carry_out`**, **`trunc_mul_loses_value`**, `forty_safe`, `fortyone_unsafe` |
| 54-trit Karatsuba | **`radical_sum_overflows`**, `wrapped_radical_changes_middle`, `radical_wrap_witness` |
| `T81BigInt` fallback | **`fallback_violates_invariant`**, `fastpath_repairs_invariant`, `three_pow_80_needs_81_trits` |

Bold entries are the ones that *refute* a property the code relies on.

## What is deliberately not here

Two claims that belong to this file are proved in the SMT layer instead, because
this Lean build cannot discharge universally-quantified `Int` division goals
without extra `Int.ediv` lemmas (and `norm_num`/Mathlib are not available):

* column value-preservation `a+b+c = digit + 3·carry` → Z3 `int.add_column.value`
* the floor-div-3 repair is legal for **every** integer → Z3 `int.add_column.legal`

They are still machine-checked; just by a different checker.

## Verified result

```
errors: 0
sorries: 0
theorems: 20
```
