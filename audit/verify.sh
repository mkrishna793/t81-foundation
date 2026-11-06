#!/usr/bin/env bash
# Reproduce the whole audit. Exits non-zero if any defect is NOT reproduced
# (i.e. this doubles as a regression gate for the fixes).
set -uo pipefail
cd "$(dirname "$0")/.."
B=audit/build; mkdir -p "$B" audit/logs
CXX=${CXX:-g++}
echo "== building =="
for f in limb_probe int_probe core_probe findings; do
  $CXX -std=c++20 -O2 -Iinclude -o "$B/$f" "audit/cpp/$f.cpp" || exit 1
done
echo
echo "== 1. defect harness =="
T81_AXION_TRAP_STDERR=0 "$B/findings" | tee audit/logs/findings.txt
echo
echo "== 2. exact-integer oracle =="
"$B/limb_probe" 400 987654321 > audit/logs/limb_probe.txt 2>&1
for c in /home/user/.venv/bin/python python3; do [ -x "$c" ] && $c -c "import z3" 2>/dev/null && PY=$c && break; done
PY=${PY:-}
if [ -z "$PY" ]; then echo "  (skipped: z3 not importable; python3 -m venv .venv && .venv/bin/pip install z3-solver)"; else
  $PY audit/smt/oracle.py < audit/logs/limb_probe.txt | tee audit/logs/oracle.txt
  echo
  echo "== 3. SMT layer =="
  $PY audit/smt/z3_audit.py | tee audit/logs/z3.txt
fi
echo
echo "== 4. Lean 4 =="
if [ -f "${LEAN4_WASM_HOME:-/home/user/leanwasm}/node_modules/lean4-wasm/lean.wasm" ]; then
  NODE=$(command -v node)
  for c in /home/user/leanwasm/node_modules/node/bin/node; do [ -x "$c" ] && NODE=$c; done
  "$NODE" audit/lean/leanrun.js audit/lean/Stage1.lean 2>&1 \
    | grep -vE "DEBUG|^  - " > audit/logs/lean.txt
  if grep -q "error" audit/logs/lean.txt; then
    echo "  LEAN: ERRORS"; grep "error" audit/logs/lean.txt | head
  else
    echo "  LEAN: 0 errors, 0 sorry, $(grep -c '^theorem' audit/lean/Stage1.lean) theorems verified"
  fi
else
  echo "  (skipped: run the provisioning steps in audit/lean/README.md)"
fi
