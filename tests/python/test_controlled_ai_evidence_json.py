"""Check that security evidence survives JSON serialization without losing verdicts."""
import json
import subprocess
import sys

raw = subprocess.check_output([sys.argv[1], "--emit-json"], text=True)
records = json.loads(raw)
assert len(records) == 1
record = records[0]
assert record["authorization_verdict"] == {
    "kind": "Quarantine", "kind_value": 4, "reason": 'policy "decision"\nwith\\context\t'
}
assert record["metadata"]["model_hash"] == 'model"\\\n\t'
assert record["metadata"]["final_authorization"] == "true"
assert record["metadata"]["authorization_status"] == "evaluated"
assert record["metadata"]["consent_status"] == "unverified"
assert record["inference_dispatched"] is False
assert record["policy_authorized"] is False
assert record["user_consent"] is False
assert record["failure_reason"].startswith("ERR_AI_AUTHORIZATION_DENIED: Quarantine:")
assert "private prompt" not in raw
print("PASS: exact verdict/context/reason round trip through JSON")
