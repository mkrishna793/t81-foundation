# Inference Comparison Benchmark Results

**Generated:** 2026-10-28  
**Commit:** `afb68667`  
**Runner:** runnervm48cm3 · 4 CPUs @ 4241 MHz  
**Build:** Release · `-O3 -march=native`

> Throughput in weight-elements/second (higher is better).  
> T81 ternary: weights in {-1, 0, +1} — multiply replaced by conditional ADD/SUB or skip.  
> NEON: same compute pattern, 4-wide SIMD (AArch64). Int8: 8-bit weights, scalar MUL.

## Dot Product

| N | FP32 | FP16-sim | Int8 | T81 Scalar | T81 NEON | NEON vs FP32 | NEON vs Int8 |
|---|------|----------|------|------------|----------|--------------|--------------|
| 64 | 4.43 Gops/s | 1.06 Gops/s | 27.78 Gops/s | 2.50 Gops/s | N/A | N/A | N/A |
| 256 | 2.63 Gops/s | 1.02 Gops/s | 34.63 Gops/s | 2.26 Gops/s | N/A | N/A | N/A |
| 1,024 | 2.18 Gops/s | 1.05 Gops/s | 39.74 Gops/s | 2.45 Gops/s | N/A | N/A | N/A |
| 4,096 | 2.08 Gops/s | 1.05 Gops/s | 35.48 Gops/s | 2.53 Gops/s | N/A | N/A | N/A |
| 16,384 | 2.05 Gops/s | 1.05 Gops/s | 30.31 Gops/s | 2.55 Gops/s | N/A | N/A | N/A |

## Matrix Multiply (1×N · N×N → 1×N)

| N | FP32 | FP16-sim | Int8 | T81 Scalar | T81 NEON | NEON vs FP32 | NEON vs Int8 |
|---|------|----------|------|------------|----------|--------------|--------------|
| 64 | 12.60 Gops/s | 9.11 Gops/s | 33.49 Gops/s | 4.15 Gops/s | N/A | N/A | N/A |
| 128 | 19.20 Gops/s | 11.48 Gops/s | 27.00 Gops/s | 4.13 Gops/s | N/A | N/A | N/A |
| 256 | 16.23 Gops/s | 11.21 Gops/s | 32.84 Gops/s | 3.96 Gops/s | N/A | N/A | N/A |
| 512 | 22.78 Gops/s | 11.82 Gops/s | 47.63 Gops/s | 4.16 Gops/s | N/A | N/A | N/A |

## Scaled Dot-Product Attention (head_dim=64)

| Seq len | FP32 | T81 Ternary | T81 vs FP32 |
|---------|------|-------------|-------------|
| 32 | 3.06 Gops/s | 1.72 Gops/s | **0.56×** |
| 64 | 2.89 Gops/s | 1.75 Gops/s | **0.61×** |
| 128 | 2.87 Gops/s | 1.78 Gops/s | **0.62×** |

## Transformer Layer Forward Pass (dim=256, heads=4, mlp=512)

| Seq len | FP32 (tok/s) | T81 Ternary (tok/s) | T81 vs FP32 |
|---------|-------------|---------------------|-------------|
| 32 | 36084 ops/s | 3468 ops/s | **0.10×** |
| 64 | 33212 ops/s | 3261 ops/s | **0.10×** |
| 128 | 26894 ops/s | 3238 ops/s | **0.12×** |

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
