# Audit checks, not Research Protocol 001 implementation

These files preserve source-audit evidence against merged PR #563, commit
`f6d4d680709c5c6c98b305628e04346f5ef61620`.

- `production.patch`: complete base-to-head production diff with zero context lines, not a proposed patch.
- `test_nc01_contract.py`: explicit fixture and statistical arithmetic checks.
  No general JCS implementation, signature check, or benchmark verifier.
- `backend_policy_probe.cpp`: vulnerability reproducer. **Exit 0 means the unsafe
  behavior was reproduced**, not that enforcement passed. It deliberately stays
  outside CMake/CTest so it cannot be mistaken for a product acceptance test.
- `*-tests.txt`, `backend-probe.txt`: captured outputs from this audit.

Run fixtures with `python3 experiments/research_001/audit/test_nc01_contract.py -v`.
Compile the probe after building `test_real_direct_api` (replace `BUILD_DIR`):

```sh
clang++ -std=c++23 -I include \
  experiments/research_001/audit/backend_policy_probe.cpp \
  BUILD_DIR/src/direct_api/libt81_direct_api.a \
  BUILD_DIR/libt81_axion.a BUILD_DIR/libt81_core.a -o /tmp/t81-backend-probe
/tmp/t81-backend-probe
```

This exact link command was run on macOS ARM64; archive names and compiler flags
need adaptation for other platforms. See [the report](../POST_MERGE_AUDIT.md)
for limitations, actual build configuration and the full readiness decision.
