# PR #563 post-merge audit

Date: 2026-10-24 (America/New_York). Verdict: **NOT READY** to freeze.

This is an independent source and methodology audit, not experimental evidence
for Research Protocol 001. No benchmark harness, verifier, workload or new
architecture is implemented by this correction. The scope follows the project's
runtime stability, explicit policy contracts and newcomer clarity priorities.

## Evidence boundary and scope expansion

Compared the complete tree diff from PR base
`ec1d99fc2c1c505c3eb058542c0684c3ecf60364` to head
`c2553a9f9e88a4f9890ee0097593e0c994a26bc1`; audited merged tree
`f6d4d680709c5c6c98b305628e04346f5ef61620`. There are 15 changed files:
11 research documents and exactly the four production files below (91 additions,
37 deletions overall). No other production hunks are present.

[PR #563](https://github.com/t81dev/t81-foundation/pull/563) describes methodological
corrections only. Its first commit (`a2d1b24c`) is documentation-only; subsequent
commits (`7984fa91`, `32263cc7`, `c2553a9f`) add build/backend repairs. The fetched
PR discussion contains no explicit request authorizing production changes.
The charter itself prohibits production changes (`RESEARCH_CHARTER.md:60-62`).
This establishes out-of-scope work against the recorded contract, not intent or
proof about private authorization. The NC-01 self-reference was already flagged
in [the pre-merge review](https://github.com/t81dev/t81-foundation/pull/563#discussion_r4367271787)
and remained in the merged tree.

All line references below refer to the merged tree unless labeled corrected.

## Production diff: recommendations for every hunk

| Change | Necessity and correctness | Recommendation |
| --- | --- | --- |
| `CMakeLists.txt:1961`: install/export `t81_math_quantization` | Required by the new static CLI link dependency; exporting a static target must preserve its link closure. No runtime policy change. | **Retain**. |
| `CMakeLists.txt:1967`: install/export `t81_decode` | Same link-closure requirement for the decoder dependency. | **Retain**. |
| `src/tools/CMakeLists.txt:62`: CLI privately links quantization and decoder | `ai_cli_shared.cpp:8-9` uses both APIs; the metrics source also uses quantization. Makes dependencies available to CLI-library consumers including TUI. Static PRIVATE links propagate as LINK_ONLY in exports. Adds a static cycle because decoder already links CLI (`:76`); CMake supports repeated static archives, but future shared-library conversion needs a separate design review. | **Retain**, with native platform CI confirmation. No broad dependency refactor in this correction. |
| `src/direct_api/CMakeLists.txt:6`: compile controlled backend | Supplies constructor/inference symbols needed by Direct API, but activates the unsafe implementation described below. This is a build repair, not a completed security integration. | **Retain only with security remediation before promoting this API as governed**; do not roll back just this source while leaving its callers linked. |
| `src/direct_api/CMakeLists.txt:12-14`: replace nonexistent backend/weights/CanonFS target names with model/CLI/VM targets | Old names are bare linker library names, not declared CMake targets. Model loading is provided by `t81_tool_model`; core already supplies CanonFS. Existing tracing dependency retained. CLI/VM add coupling but no new subsystem. | **Retain** valid target replacements; avoid restoring nonexistent libraries. |
| `controlled_ai_backend.cpp:101`: `operation` → `syscall` | Matches actual `SyscallContext` (`include/t81/axion/context.hpp:12-31`). Keeps existing `ai.inference.inference` naming; does not establish a syscall naming contract. | **Retain** API repair; correct authorization context separately. |
| `controlled_ai_backend.cpp:102` deletion of hardcoded consent | The removed member does not exist. Hardcoding true could never establish consent anyway. There is no replacement consent input or enforcement. | **Retain deletion**; never restore fabricated consent. Explicit consent remains unresolved. |
| `controlled_ai_backend.cpp:104-110`: `allowed` → `kind == Allow` for recording/return | Matches `Verdict` (`include/t81/axion/verdict.hpp:8-13`) and classifies Deny, Defer, Warn, Quarantine conservatively. It does not make the caller enforce that result. | **Retain** API repair; acceptance tests must exercise the caller. |

No hunk is necessary for documentation corrections. Mechanical reversal of all
four files would restore known build failures rather than establish a sound
baseline. The narrow corrective PR therefore repairs documentation and preserves
reproducers; production authorization remediation must be explicitly reviewed.
This is not a claim that the activated API is safe to ship.

## Production security and behavior findings

**P1 — verdicts do not gate execution.** `controlled_ai_backend.cpp:125-158`
assigns `check_policy_consent()` to evidence and then runs inference regardless.
A null engine, Deny, Defer, Warn and Quarantine all permit successful simulated
inference. Both the local and external paths currently synthesize strings
(`:163-205`); this audit did not observe a real network request. The defect
predates #563 in source but #563 makes the backend linkable in Direct API.
Do not describe this as a new Axion engine regression or an observed exfiltration.
Before enabling real effects, reject every non-Allow verdict before dispatch.

**P1 — approval is mislabeled as consent; final authorization is absent.**
`controlled_ai_backend.cpp:98-111,132` supplies only a syscall string. Caller,
payload and trace evidence are empty, opcode is Nop, and
`final_authorization=false`. The engine permits an absent policy in this mode
(`kernel/axion/policy_engine.cpp:245-253`), whereas final authorization denies it
and completes evidence checks (`:217-221,320-355`). `ctx.policy` also stays null
and can mislead ethics checks even if the engine owns a policy. Policy approval
is not user consent. The request has no consent proof and the factory ignores
`AIBackendConfig.require_user_consent` and `evidence_collection`
(`include/t81/ai_backend/controlled_ai_backend.hpp:44-49`, implementation `:227-236`).
A follow-up must define consent evidence and supply truthful context using
existing authorization contracts; until then this path is not a valid governed
reference candidate. Adding invented consent fields to Axion is not warranted.

**P1 — policy evidence belongs to the wrong operation.**
`controlled_ai_backend.cpp:106-109` appends to `evidence_log_.back()` before the
current record is pushed (`:158`). First operation records no policy decision;
subsequent checks mutate the previous operation's record. This also predates
#563. Log into the current operation, preserve the exact verdict kind, and only
finalize evidence after the authorization outcome is known.

**P2 — existing backend tests do not protect the integration.**
`tests/cpp/controlled_ai_backend_test.cpp:25` calls nonexistent
`PolicyEngine::parse_policy`; the file is not registered by CMake. Existing
Axion/VM tests test different paths. A green CTest result cannot validate this
wrapper's consent or enforcement. Direct API also has placeholder hashes
(`src/direct_api/t81_direct_api.cpp:226-243`) and generates synthetic decisions;
its benchmark executable is not evidence of governed inference.

The draft intentionally does not invent a new consent interface or silently
change Direct API behavior inside a research documentation correction. These
P1 findings remain open production remediation requirements, with a standalone
reproducer in `audit/backend_policy_probe.cpp`.

## Research freeze blockers

1. **P1: Acceptance contradicts denial evidence.**
   `BASELINE_SPECIFICATION.md:88` and RFC-00D4 §5 require verifier verdict ALLOW;
   `EXPERIMENT_PROTOCOL.md:25-28,83-90` and NC-06 require valid DENY/missing-policy
   results. `STOPPING_CRITERIA.md:28` requires verification success for every
   valid bundle. Define separate evidence validity, authorized execution,
   correctly denied execution, and intentionally corrupt evidence outcomes.
   Also reconcile schema-required output digests for denied operations.

2. **P1: Digest/signature preimages are inconsistent.**
   Original baseline `:86` hashes `meaning`; NC-05 hashes `meaning.parameters`.
   This correction aligns those two to JCS(parameters). Baseline `:87` signs a
   bundle digest, while RFC-00D4 §4/§5 signs meaning + evidence + authority.
   Whole-bundle signatures must exclude the signature itself or be detached.
   Freeze the exact signed bytes, key trust source, hash encodings and policy
   snapshot coverage. Removing NC-01's self-reference does not resolve all this.

3. **P1: NC-03 cannot detect all unrecorded state changes.**
   `NEGATIVE_CONTROLS.md:76-78` assumes recomputing a hash of an unchanged log
   detects memory changes omitted from that log. It does not. Require an
   independent state/output oracle or bounded replay and define what is
   compared. A trusted trace tap versus a compromised recorder are different
   threat models. Merely hashing producer assertions is not independent replay.

4. **P1: NC-07 lacks authenticated freshness/commit context.**
   `NEGATIVE_CONTROLS.md:86-88` backdates evaluation to before revocation and
   expects rejection, but that timestamp alone can describe a legitimate
   pre-revocation authorization. Detached bundles lack a trusted revocation
   history, commit order and freshness authority. A malicious host may forge
   both clocks and policy state. Define trusted inputs and the linearization
   point; otherwise narrow the claim to a cooperative injected stale timestamp.

5. **P1: NC-08 assumes a boundary outside a malicious host's control.**
   `NEGATIVE_CONTROLS.md:91-92` assumes storage verifies signed ALLOW, without
   defining whether host, signer and SQLite/filesystem share authority. A host
   controlling keys or storage can forge or bypass the check; a valid ALLOW can
   also be replayed unless bound to action, payload, policy epoch and transaction.
   HMAC fallback also changes the public-auditor trust model: a verifier holding
   the shared secret can forge producer evidence. Keep that mode separate from
   public-key claims. Specify independent trust and replay assumptions or narrow this to FFI-return
   corruption with a trusted signer/storage boundary. No new infrastructure is
   justified by this protocol audit.

6. **P1: Cross-platform whole-bundle equality has no deterministic preimage.**
   `MEASUREMENT_PLAN.md:26` and `STOPPING_CRITERIA.md:30,41` require identical
   whole-bundle digests. RFC-00D4 §4 requires timestamps and runtime metadata;
   timing, environment, signatures and identifiers can vary without semantic
   divergence. Freeze a deterministic result projection, logical time/ID rules,
   and same-arm cross-platform pairing; keep measurement metadata separate.

7. **P1: Statistical decisions are not executable or valid equivalence tests.**
   `MEASUREMENT_PLAN.md:43-50`, `HYPOTHESES.md:88-90` leave `CI_95_max` undefined,
   offer post-hoc method choices and use Mann-Whitney as a test of means.
   [The test concerns distributions](https://docs.scipy.org/doc/scipy/reference/generated/scipy.stats.mannwhitneyu.html),
   not a general equality-of-means hypothesis. A
   nonsignificant difference does not establish equal/lower latency for Outcome
   B. Predeclare estimand, comparison units, paired/block design, equivalence or
   noninferiority margin, confidence procedure and multiplicity treatment.
   `STOPPING_CRITERIA.md:45-46` permits repeated CI-width stopping without a
   maximum N or sequential correction. Use a fixed preregistered N or a valid
   sequential design. Zero-failure bounds 9.50%/0.99% are **one-sided** 95%
   Clopper-Pearson bounds (`1 - 0.05^(1/N)`); a central two-sided 95% interval
   has upper bounds about 11.57%/1.22%. State the convention and iid assumptions.
   Repeating a deterministic input is not independent adversarial coverage.
   For N=300 the one-sided value is 0.9936%, so round to 0.994% rather than
   asserting the literal inequality <=0.99%.

8. **P2: Sampling and endpoints disagree.**
   `MEASUREMENT_PLAN.md:39-42` expands safety cases 01–04, whereas
   `EXPERIMENT_PROTOCOL.md:81` expands all eight. The 100-file corpus lacks a
   mapping to those cases and N. The verification threshold is <1000 ms but
   falsification is >1000 (`STOPPING_CRITERIA.md:29`), leaving equality undefined.
   Predeclare denominators, failures/timeouts and exact threshold boundaries.
   `REPRODUCIBILITY.md:80` requires sample_count >=30 even when early failure
   occurs before 30 trials; the manifest must represent those terminal results.

9. **P1: Complexity accounting can preselect a winner.**
   `MEASUREMENT_PLAN.md:29` counts C/C++ VM+policy LOC, but the conventional arm
   uses Rust Wasmtime, Go/Rego OPA and SQLite. Counting only glue or excluding
   non-C/C++ dependencies makes the 5x criterion incomparable. Define identical
   component/language boundaries and separate maintained glue from total
   transitive implementation. In-process OPA is an aspiration, not a pinned
   C/Rust API artifact: name the actual integration (e.g. Wasm-compiled Rego),
   build/version, builtins and compilation/cache costs before freezing it.
   [OPA documents compiled Rego/Wasm and its ABI](https://www.openpolicyagent.org/docs/wasm),
   including host-provided builtins; the protocol must identify its chosen artifact.
   Keep H3 transport/serialization factors controlled per arm; Arm (i)'s IPC
   overhead cannot establish an intrinsic architectural advantage.

10. **P2: Measurement boundaries are underspecified.**
    `MEASUREMENT_PLAN.md:24,27,86-93` measures ingestion-to-commit/denial, but
    protocol Step 3 does not specify whether signing, evidence serialization,
    startup, compilation and fsync are timed. A tmpfs commit tests logical
    acceptance, not crash durability. RSS at completion is not necessarily peak
    RSS; `/proc/self/statm` and `getrusage` have different meanings and units
    across OSes. Specify process reset, memory method, warmup/state reset,
    key reuse, persistence semantics, thread/core limits and arm order. Protocol
    `:75` pins cores 2,3 while baseline `:56` requires one core.

11. **P1: Numeric and sandbox contracts are incomplete.**
    `MEASUREMENT_PLAN.md:80` includes 2^63-1, outside exact binary64 integer
    representation; JCS requires a defined string representation for exact large
    integers. Signed C++ overflow, Wasm wraparound and Python arbitrary precision
    also need identical checked arithmetic, currency scale and invalid-input
    behavior. Baseline `:60` constrains spending_limit rather than explicitly
    comparing computed V to current L. NC-02's hardcoded `/tmp` path is Linux
    specific; define unauthorized-write observation from outside the guest and
    distinguish denial of payload writes from permitted audit-log writes.

12. **P1: Claims and readiness metadata overstate current evidence.**
    `CLAIM_REGISTER.md:37` marks CLM-06 supported by a verifier that does not
    exist in this protocol. Existing unrelated replay tests do not prove this
    claim. H1's necessity claim and proposal's 'empirical proof' exceed a single
    workload and baseline (`HYPOTHESES.md:32`, `RESEARCH_PROPOSAL.md:66-68`).
    Passing one Wasmtime configuration cannot falsify all integrated-VM value;
    a baseline bug cannot prove custom ISA necessity. Bound conclusions to
    observed implementations/configurations. H1's disjunctive falsifiers do not
    align with Outcome B's conjunction of latency and 5x LOC. Reconcile these
    before classifying outcomes. Most statuses say Approved while proposal says
    Under Review; reproduction commands invoke nonexistent run.py/verify.py.
    Label these planned, not validated or operational. Raw artifact storage in
    tmpfs (`REPRODUCIBILITY.md:24`) also needs a durable export/availability rule.

## NC-01 correction and reference audit

Corrected `NEGATIVE_CONTROLS.md:51-56` relies on raw-byte equality with a genuine
JCS serialization. It removes the embedded whole-file digest requirement and
clarifies that rejecting noncanonical wire bytes is this experiment's extra
profile, not a requirement imposed by RFC 8785. Valid reordering leaves the
canonical-data signature valid. Malformed JSON fails parsing; a canonical
semantic change passes formatting but fails its unchanged input digest.
Duplicate names and other JCS-domain failures are classified separately.

Repository-wide search found explicit NC-01 references in NEGATIVE_CONTROLS,
TRACEABILITY and RESEARCH_CHARTER. Both traceability rows now cite actual audit
fixtures rather than nonexistent `verify.py` tests. Generic negative-control
references in STOPPING_CRITERIA/EXPERIMENT_PROTOCOL remain applicable; the
protocol Step 3 whole-output hash is a measurement and is compatible with the
removed embedded digest check. Baseline input-hash scope now matches NC-05.
The existing error name is retained for compatibility and explained precisely.

Source: [RFC 8785 §§3.1–3.2](https://www.rfc-editor.org/rfc/rfc8785#section-3).
Audit fixtures intentionally cover explicit ASCII/small-integer examples only;
they do not claim a general JCS implementation or cryptographic verifier.

## Required before freeze

Resolve the blockers above in protocol text, review the exact verification and
trust preimages, preregister fair measurement/statistical rules, and replace
unsupported evidence statuses with traceable evidence or untested labels.
Resolve the activated backend's authorization/consent defects in an explicitly
scoped production change before using it as a trusted governed candidate.
Do not start Protocol 001 implementation or infer architectural outcomes now.

## Focused validation and actual results

Host: macOS ARM64, Darwin 25.5.0, Apple Clang 21.0.0. Isolated worktree;
original user checkout and its untracked build directory were preserved.

Configuration actually used (existing local dependency sources avoided network):

```sh
cmake -S /private/tmp/t81-pr563-audit -B /private/tmp/t81-pr563-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DT81_BUILD_TESTS=ON -DT81_BUILD_TUI=ON \
  -DT81_BUILD_EXAMPLES=OFF -DT81_BUILD_BENCHMARKS=OFF \
  -DT81_ENABLE_TERNARYOS=OFF -DT81_ENABLE_DPE=OFF \
  -DFETCHCONTENT_SOURCE_DIR_ASIO=/Users/t81dev/Code/t81-foundation/build/_deps/asio-src \
  -DFETCHCONTENT_SOURCE_DIR_FTXUI=/Users/t81dev/Code/t81-foundation/build/_deps/ftxui-src
```

The initial configure without cached Asio sources failed on sandbox DNS access;
this was an environment failure, not a CMake regression. All tests below used
the successful cached-source configuration.

| Check actually executed | Result |
| --- | --- |
| Base backend `clang++ -std=c++23 -I include -fsyntax-only` | **FAIL**, four nonexistent-member errors: operation, user_has_explicit_consent, allowed twice. Confirms the need for API repair when adding this source. |
| Base `cmake --build ... --target test_real_direct_api --parallel 6` (tests/TUI/OS/DPE off) | **FAIL** at link: `ld: library 't81_ai_backend' not found`; old link command also contains `-lt81_weights -lt81_canonfs`. |
| Base source with only #563 CLI dependency hunk applied, separate configure | **FAIL** generation: CLI requires quantization/decoder targets not in any export set. Confirms the two install/export additions are needed together with the CLI change. |
| Merged configure, build Direct API executable, CLI, TUI snapshot executable and six policy/decoder targets | **PASS** compilation/link. Apple ld emits duplicate-archive warnings, including the new CLI/decoder cycle. Not a warning-free build. |
| CTest regex for axion_ai_hooks_test, canonfs_policy_boundary_test, t81_vm_ai_predispatch_deny_audit_test, axion_policy_conformance_matrix_test, axion_policy_bytecode_test, with --timeout 45 | **5/5 PASS**, 1.37 s total. See `audit/policy-tests.txt`. |
| `tui_snapshot_test` executed directly | **PASS**, 12 printed checks; exit 0. CTest `-N -R tui` lists zero tests in this configuration, so this was not counted as a CTest pass. |
| `t81_vm_decoder_test` via CTest --timeout 30 | **TIMEOUT**, 30.01 s. Earlier unbounded invocation was interrupted. |
| Same decoder CTest outside sandbox, --timeout 45 | **TIMEOUT**, 45.03 s. This is unresolved, not a sandbox-only explanation or a demonstrated #563 regression. Decoder/test source is unchanged by #563; the link graph changed. Native baseline attribution is still required. |
| Audit backend probe compiled against merged Direct API/Axion/core archives | **Unsafe behavior reproduced** for all four non-Allow verdicts and null engine; actual PolicyEngine(nullopt) returns successful inference with consent=true. A second operation appends its decision to the previous record. Exit 0 confirms reproduction, not enforcement. See `audit/backend-probe.txt`. |
| `python3 .../audit/test_nc01_contract.py -v` | **7/7 PASS**. Fixtures separate valid reordering, whitespace, syntax error, semantic digest change and duplicate-name handling; arithmetic confirms one-sided confidence bounds. |
| `python3 scripts/ci/check_ai_experiment_boundary.py` | **PASS**, no AI sandbox boundary violations. This existing checker is not a docs-only scope gate for Research Protocol 001. |
| `python3 scripts/ci/check_ai_backend_adapter_contract.py --out-dir /private/tmp/t81-pr563-adapter` | **PASS**. This checks a separate adapter contract, not the controlled wrapper's enforcement. |
| `git diff --check` | **PASS**. |

The Direct API development executable was built but not run: its main hardcodes
a user model path and `/tmp/test_policy.apl` and reports a claimed speedup against
a fixed 3200 ms number (`development_tests/test_real_direct_api.cpp:18-20,49-52`).
It is not a portable acceptance test. No experiment runs or architectural
conclusions were generated. Full production diff: `audit/production.patch`.

Portability review: the retained hunks add no platform syscalls or architecture
intrinsics; target names and enum/member corrections are portable C++. Linux
static archive repetition and Windows library resolution still need native CI;
only macOS ARM64 was executed here. The newly cyclic static targets cannot be
assumed valid as shared libraries. Linux x86_64/ARM64 and Windows runtime/link
results are **unverified**, not inferred from the local pass. Draft status is
intentional given these limits and the unresolved decoder timeout.
