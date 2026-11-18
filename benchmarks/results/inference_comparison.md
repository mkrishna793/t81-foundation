# Inference Comparison Benchmark Results

**Generated:** 2026-11-18  
**Commit:** `7c8b4b5a`  
**Runner:** runnervm8axu0 · 4 CPUs @ 3296 MHz  
**Build:** Release · `-O3 -march=native`

> Throughput in weight-elements/second (higher is better).  
> T81 ternary: weights in {-1, 0, +1} — multiply replaced by conditional ADD/SUB or skip.  
> NEON: same compute pattern, 4-wide SIMD (AArch64). Int8: 8-bit weights, scalar MUL.

## Dot Product

| N | FP32 | FP16-sim | Int8 | T81 Scalar | T81 NEON | NEON vs FP32 | NEON vs Int8 |
|---|------|----------|------|------------|----------|--------------|--------------|
| 64 | 1.90 Gops/s | 714.12 Mops/s | 7.95 Gops/s | 1.21 Gops/s | N/A | N/A | N/A |
| 256 | 1.36 Gops/s | 821.88 Mops/s | 8.46 Gops/s | 1.33 Gops/s | N/A | N/A | N/A |
| 1,024 | 1.12 Gops/s | 836.82 Mops/s | 8.56 Gops/s | 1.36 Gops/s | N/A | N/A | N/A |
| 4,096 | 1.08 Gops/s | 839.02 Mops/s | 8.63 Gops/s | 1.37 Gops/s | N/A | N/A | N/A |
| 16,384 | 1.07 Gops/s | 826.12 Mops/s | 8.64 Gops/s | 1.37 Gops/s | N/A | N/A | N/A |

## Matrix Multiply (1×N · N×N → 1×N)

| N | FP32 | FP16-sim | Int8 | T81 Scalar | T81 NEON | NEON vs FP32 | NEON vs Int8 |
|---|------|----------|------|------------|----------|--------------|--------------|
| 64 | 10.01 Gops/s | 2.32 Gops/s | 9.89 Gops/s | 1.19 Gops/s | N/A | N/A | N/A |
| 128 | 10.72 Gops/s | 2.37 Gops/s | 10.27 Gops/s | 1.27 Gops/s | N/A | N/A | N/A |
| 256 | 11.24 Gops/s | 2.39 Gops/s | 10.48 Gops/s | 1.32 Gops/s | N/A | N/A | N/A |
| 512 | 11.66 Gops/s | 2.39 Gops/s | 10.58 Gops/s | 1.34 Gops/s | N/A | N/A | N/A |

## Scaled Dot-Product Attention (head_dim=64)

| Seq len | FP32 | T81 Ternary | T81 vs FP32 |
|---------|------|-------------|-------------|
| 32 | 1.49 Gops/s | 818.44 Mops/s | **0.55×** |
| 64 | 1.48 Gops/s | 825.02 Mops/s | **0.56×** |
| 128 | 1.42 Gops/s | 839.97 Mops/s | **0.59×** |

## Transformer Layer Forward Pass (dim=256, heads=4, mlp=512)

| Seq len | FP32 (tok/s) | T81 Ternary (tok/s) | T81 vs FP32 |
|---------|-------------|---------------------|-------------|
| 32 | 16579 ops/s | 2065 ops/s | **0.12×** |
| 64 | 14836 ops/s | 2011 ops/s | **0.14×** |
| 128 | 12486 ops/s | 1969 ops/s | **0.16×** |

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
