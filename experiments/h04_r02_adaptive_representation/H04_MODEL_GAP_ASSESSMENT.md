# T81 — H04-R02 Model Gap Assessment

**Date:** 2026-10-29  
**Disposition:** **MODEL SUFFICIENT** for the scoped, conditional four-arm symbolic comparison.  
**Independent constraint:** **HARDWARE PARAMETERS UNAVAILABLE** for physical performance/energy conclusions.  
**Authorization:** Read-only evidence assessment; no new benchmark or measurement authorized.

## 1. Decision and scope

The corrected H04-R02 packet already answers the bounded symbolic question posed by `docs/research/TERNARY_ADVANTAGE_DECISION_MATRIX.md`: a four-arm comparison exists, both adaptive schedules are optimized under their declared model, and conditional crossover regions and adversarial traces are reported. Do not commission another scheduler implementation or a duplicate symbolic sweep solely to recreate this result.

The result is **not** a universal ternary advantage: its binary comparators are specified fixed-width tier families, not the set of all binary encodings; its costs are stipulated abstract units U, not observed time, energy, or silicon area. In particular, the actual Base-243 codec's payload advantage against fixed 10-bit fields is not radix-exclusive: binary software can encode and decode precisely the same byte language.

## 2. Source identity and provenance

Source: `t81dev/t81-foundation`, default branch `main`, inspected 2026-10-29. H04 corrective PR [#572](https://github.com/t81dev/t81-foundation/pull/572) is merged, merge commit `6864a810693a6034bd28be870ee38a180ba70ba0`.

| Source path (relative to `experiments/h04_r02_adaptive_representation/`) | Inspected Git blob SHA-1 | Role |
|---|---|---|
| `SPECIFICATION.md` | `c78612e569474388d46e2eef4aff387c69780eff` | Four-arm contract and limits |
| `METHODOLOGY.md` | `ba7979fb41de7b66e3bc8913da0ce9f154f6899b` | Eight trace families, shared overhead |
| `MATHEMATICS.md` | `e8b83506bc8f6dd0edddf4e41526f4f2e004c106` | Domains, cost model, crossover derivation |
| `RESULTS.md` | `fdb70b1dcfdcdb995befd9df2e96ebcd8d7f8a76` | Unified corrected conclusion |
| `CORRECTED_RESULTS.md` | `49624f6b11f0f06c2b2e62f13b77fca79ae9e3dd` | Eight-workload totals and crossover summary |
| `LIMITATIONS.md` | `738903eaba896985d27d951b7e31460b5e515343` | Unmeasured and unequal-capacity caveats |
| `VERIFICATION.md` | `eccdad5028574573bfd379b63428c59b4fae86bf` | Reported local test receipts |
| `PRECISION_CORRECTION.md` | `cfdb46606a9f63069335af6103ea4901af53ef30` | Shared-overhead precision fix |
| `DISCREPANCY_LEDGER.md` | `e60ecae201bd14439954527fd04b717ccbcd6da9` | Historical error reconciliation |
| `packing_results.json` | `46ae4e3b1624cbadd3119c357b9ffe4753c77382` | Actual codec byte-count results |

These are Git **blob identifiers**, not SHA-256 file digests. They identify inspected repository content; this assessment did not rerun the executable or independently reproduce prior test receipts. The decision matrix itself was inspected on `main` at blob `82c80098e4c74e57474017a7e6682a5638f9fb61`.

## 3. Gap matrix

| Requirement from decision matrix | Existing H04-R02 evidence | Finding | Remaining gap |
|---|---|---|---|
| Four arms FB/FT/AB/AT | `SPECIFICATION.md`, `MATHEMATICS.md`, `CORRECTED_RESULTS.md` | **Covered** | Fixed arms use 64-bit / 41-trit top tiers; binary/ternary tier capacities differ |
| Shared logical workload | Eight 100-value int64 traces; `METHODOLOGY.md` | **Covered within shared int64 domain** | Does not imply equal native capacity or general workload representativeness |
| Global optimal adaptive schedule | C++ prefix DP, Python suffix oracle, exact rational finite enumeration | **Covered, source-reported tests** | Offline omniscient selection; real causal decision policy untested |
| Metadata/selection/transition | `AB = S_B + Bm + τD + Nκ`; `AT = rS_T + Bm + τD + Nκ` | **Covered in symbolic model** | Conversion, planning, framing and access costs not physically calibrated |
| Crossovers and adversarial cases | Eight workloads, reoptimized crossover envelopes, schedule-changing counterexample | **Covered** | Parameter realism and cross-platform reproduction open |
| Equal-capacity binary sensitivity | 10-bit Tier-0 control for six-trit domain | **Partially covered** | 10 bits cover 729 states but include 295 additional states; not an optimal binary block code |
| Codec byte-count comparison | Reversible Base-243 vs packed fixed 10-bit, equal 8-byte count header | **Covered for those two codecs** | No equal-domain entropy-optimal binary block-code comparator with access contract |
| Random access and alignment | Mathematical offset formula; sequential decoder | **Not measured** | Index reads, alignment 1/8/64, block sizes 1/5/64/1024 |
| Hardware performance/energy | Explicitly absent | **Unavailable** | Matched device, periphery, reliability, timing, bandwidth and energy data |
| Independent execution | macOS ARM64 Apple clang 21 test receipts; exact oracle | **Reported, not reproduced in this assessment** | Independent Linux/Windows/laboratory reproduction |

## 4. Modeled costs and crossovers

`MATHEMATICS.md` defines a common abstract unit U, binary payload-bit cost 1 U, ternary trit cost `r` U, metadata `m` binary control bits per block, transition coefficient `τ`, per-value selection/conversion coefficient `κ`, `B` blocks and `N` values. The shared `Bm + Nκ` overhead is analytically canceled *before* optimizer comparisons to avoid floating-point loss at extreme scales, while retained in displayed totals.

For the default `r=1`, `m=2`, `τ=1`, `κ=0.5`, block size 1, legacy 8-bit Tier-0 model, the repository reports:

| Workload | AB total U | AT total U | Optimized total crossover r* |
|---|---:|---:|---:|
| adversarial_transition | 3397 | 2372 | 1.518987342 |
| alternating | 4147 | 2897 | 1.531914894 |
| boundary_oscillation | 1549 | 850 | 2.165000000 |
| constant_large | 6650 | 4350 | 1.560975610 |
| constant_small | 1050 | 850 | 1.333333333 |
| phased | 3253 | 2228 | 1.518987342 |
| signed_extremes | 3405 | 2359 | 1.520398010 |
| sparse_large | 1171 | 929 | 1.361194030 |

The crossover is the *optimized total-cost* equality boundary under the stipulated model, not a measured physical threshold. The 10-bit Tier-0 boundary-oscillation control instead reports AB=1250 U, AT=850 U at r=1, crossover 5/3; at r=1.60 AT=1210 U < AB=1250 U. This establishes a **conditional modeled win against the specified 10-bit control**. It does not establish a win against every possible binary codec or native binary processor.

For constant-large, the default AB=6650 U is greater than fixed binary FB=6400 U; adaptation is not universally beneficial even inside this model. No universal improvement percentage should be inferred from one trace.

## 5. Binary codec counterexample and exact information bound

Six trits have `3^6=729` possible values. The information-theoretic minimum for an unconstrained stream of N independent six-trit values is `ceil(N*log2(729))` **bits**, with a separate byte-rounding step. Five trits have `3^5=243` states and fit in one byte. The implemented Base-243 stream groups trits five per byte, giving `ceil(6N/5)` payload bytes. The fixed 10-bit binary comparator uses `ceil(10N/8)` bytes.

At N=1000, `packing_results.json` reports 1200 Base-243 payload bytes versus 1250 fixed 10-bit payload bytes, each with an additional equal 8-byte count header (1208 versus 1258 framed). These are actual encoded lengths and round trips, not throughput or timing.

**Constructive binary counterexample:** A binary computer can store the **identical 1200 Base-243 payload bytes** and run the identical encoding and decoding algorithm. Therefore no claim that Base-243 is *strictly more compact than all binary implementations* follows. The Base-243 representation is itself a binary byte encoding of ternary symbols. A general binary block-code comparator may also encode 729^N states in `ceil(N*log2(729))` bits before byte alignment, though it may incur different access and computation costs.

This does not make Base-243 useless: it may be an attractive codec under particular constraints. It does mean its observed savings relative to 10-bit fields are **codec-design savings**, not a demonstrated hardware radix advantage.

## 6. Ranked residual questions by information gain

1. **Equal-domain binary block codec and access contract — highest:** Compare fixed 10-bit, Base-243 and an integer/enumerative block encoding of 729-state values, including equal count framing, optional tags, block sizes 1/5/64/1024, 1/8/64-byte alignment, sequential reads and random-index access. First specify a deterministic non-timed codec and round-trip oracle. The binary implementation of Base-243 is an explicit comparator/equivalence witness. This can establish representation trade-offs but not native ternary superiority.
2. **Causal adaptive selection:** Replace offline foreknowledge with a defined online policy, including classifier/planner and conversion cost. No new implementation is justified until semantics and cost-accounting are fixed.
3. **Physical parameter grounding:** Obtain comparable 2-state/3-state device area, sensing, write, reliability/ECC and energy/latency data. If unavailable, label results conditional; do not fabricate calibration.
4. **Cross-platform independent reproduction:** Reproduce H04 oracle and codec correctness on Linux/Windows using pinned toolchains. This is an evidence-quality task, not a new performance result.

## 7. Decision and stopping rules

**Primary disposition: MODEL SUFFICIENT.** The existing H04-R02 symbolic study is sufficient for the scoped four-arm cost and crossover questions. The model already demonstrates both conditional wins and limitations, and no duplicate DP or symbolic sweep is warranted.

**Separate physical disposition: HARDWARE PARAMETERS UNAVAILABLE.** No source-backed real-world speedup, energy, or silicon-area conclusion is available. This is a limitation, not a defect that can be fixed by rewriting the symbolic model.

**Next single scientific task:** Define the equal-domain matched codec/access contract as a *design-only protocol*, with the binary Base-243-equivalent witness and a binary integer-block comparator. Pre-register success/failure criteria and evidence classification. Stop before timed execution, PMU use, or new hardware claims. The experiment described in `RESULTS.md` is a proposal, **not yet authorized** by this assessment.

**No authorization:** No CMP3 gate change, M1–M4 approval, PR merge, measurement activation, VM/ISA expansion, or historical evidence rewrite follows from this report.

## 8. Evidence boundary

This report was prepared by inspecting published GitHub file contents and the merged PR #572 record. It did **not** independently run H04-R02 executables, inspect physical devices, obtain independent experimental timing data, or verify GitHub CI at a new commit. Reported test totals remain source-reported. All equations and finite byte counts are either explicitly derived here or attributed to the inspected packet.
