# Inference Comparison Benchmark Results

**Generated:** 2026-11-04  
**Commit:** `08326bb1`  
**Runner:** runnervm7aw5e · 4 CPUs @ 3661 MHz  
**Build:** Release · `-O3 -march=native`

> Throughput in weight-elements/second (higher is better).  
> T81 ternary: weights in {-1, 0, +1} — multiply replaced by conditional ADD/SUB or skip.  
> NEON: same compute pattern, 4-wide SIMD (AArch64). Int8: 8-bit weights, scalar MUL.

## Dot Product

| N | FP32 | FP16-sim | Int8 | T81 Scalar | T81 NEON | NEON vs FP32 | NEON vs Int8 |
|---|------|----------|------|------------|----------|--------------|--------------|
| 64 | 2.50 Gops/s | 971.02 Mops/s | 13.99 Gops/s | 1.81 Gops/s | N/A | N/A | N/A |
| 256 | 1.54 Gops/s | 956.36 Mops/s | 17.12 Gops/s | 1.82 Gops/s | N/A | N/A | N/A |
| 1,024 | 1.29 Gops/s | 973.81 Mops/s | 18.11 Gops/s | 1.81 Gops/s | N/A | N/A | N/A |
| 4,096 | 1.24 Gops/s | 979.67 Mops/s | 18.40 Gops/s | 1.82 Gops/s | N/A | N/A | N/A |
| 16,384 | 1.22 Gops/s | 979.08 Mops/s | 18.47 Gops/s | 1.83 Gops/s | N/A | N/A | N/A |

## Matrix Multiply (1×N · N×N → 1×N)

| N | FP32 | FP16-sim | Int8 | T81 Scalar | T81 NEON | NEON vs FP32 | NEON vs Int8 |
|---|------|----------|------|------------|----------|--------------|--------------|
| 64 | 7.28 Gops/s | 5.25 Gops/s | 20.77 Gops/s | 3.38 Gops/s | N/A | N/A | N/A |
| 128 | 14.93 Gops/s | 5.48 Gops/s | 21.84 Gops/s | 3.52 Gops/s | N/A | N/A | N/A |
| 256 | 16.22 Gops/s | 5.63 Gops/s | 22.16 Gops/s | 3.60 Gops/s | N/A | N/A | N/A |
| 512 | 14.59 Gops/s | 5.72 Gops/s | 22.44 Gops/s | 3.27 Gops/s | N/A | N/A | N/A |

## Scaled Dot-Product Attention (head_dim=64)

| Seq len | FP32 | T81 Ternary | T81 vs FP32 |
|---------|------|-------------|-------------|
| 32 | 1.88 Gops/s | 966.84 Mops/s | **0.51×** |
| 64 | 1.86 Gops/s | 961.85 Mops/s | **0.52×** |
| 128 | 1.59 Gops/s | 958.72 Mops/s | **0.60×** |

## Transformer Layer Forward Pass (dim=256, heads=4, mlp=512)

| Seq len | FP32 (tok/s) | T81 Ternary (tok/s) | T81 vs FP32 |
|---------|-------------|---------------------|-------------|
| 32 | 20593 ops/s | 2529 ops/s | **0.12×** |
| 64 | 16954 ops/s | 2515 ops/s | **0.15×** |
| 128 | 15329 ops/s | 2406 ops/s | **0.16×** |

## Weight Loading — Memory Efficiency

| Weights | FP32 bytes | FP16 bytes | Int8 bytes | Ternary bytes | Ternary/FP32 |
|---------|------------|------------|------------|---------------|--------------|
| 1,024 | 4096 | 2048 | 1024 | 256 | 0.06× |
| 4,096 | 16384 | 8192 | 4096 | 1024 | 0.06× |
| 16,384 | 65536 | 32768 | 16384 | 4096 | 0.06× |
| 65,536 | 262144 | 131072 | 65536 | 16384 | 0.06× |

## Key Takeaways

- **NEON ternary dot product:** no-multiply 4-wide SIMD (vcgtq_f32 + vbslq_f32).
  Beats scalar FP32 at large N once working set exceeds L1 cache.
- **Int8** fills the quantization ladder between FP16 and ternary.
  SDOT/SMULL autovectorization gives very high throughput at small N.
- **Matmul:** FP32/int8 autovectorize to fused MACs; ternary NEON cmp+sel
  still trails. Next step: tiled inner loop to keep output row in registers.
- **Transformer layer:** end-to-end tokens/second through attention + SwiGLU MLP.
  Ternary weight projections (Q,K,V,O,gate,up,down) — no float multiply.
- **Memory:** ternary = 16× fewer bytes than FP32, 8× vs FP16, 4× vs Int8.
  A 7B model: FP32=28 GB, FP16=14 GB, Int8=7 GB, Ternary=1.75 GB.

## Methodology

- **FP32:** `float` multiply-accumulate via inner loop.
- **FP16-sim:** `uint16_t` weights promoted to `float` per element (no AVX-512 FP16 MACs).
- **Int8:** `int8_t` weights, scalar MUL to `int32` accumulator. Compiler autovectorizes.
- **T81 Ternary (scalar):** weights in {-1,0,+1}, branch to ADD/SUB/skip. No float multiply.
- **T81 Ternary (NEON):** same compute pattern, 4 elements/cycle via `vcgtq_f32` + `vbslq_f32`.
  No `vmulq_f32` — the ternary constraint eliminates all multiplies. AArch64 only.
- All benchmarks compiled with `-O3 -march=native`, mean of 2 repetitions.
- Results committed automatically by the `inference-bench` CI workflow.

For the VM packed-trit dispatch path see `BM_NativeWeightsExecution.cpp`.
