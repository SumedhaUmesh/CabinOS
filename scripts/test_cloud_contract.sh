#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <cloud_url>"
  exit 2
fi

CLOUD_URL="$1"

run_case() {
  local utterance="$1"
  local label="$2"
  local tmp
  tmp="$(mktemp)"

  curl -sS -X POST "${CLOUD_URL}" \
    -H "Content-Type: application/json" \
    -d "{\"session_id\":\"contract-${label}\",\"utterance\":\"${utterance}\"}" > "${tmp}"

  python3 - "${tmp}" "${label}" <<'PY'
import json, sys
path, label = sys.argv[1], sys.argv[2]
data = json.load(open(path))
def check(cond, msg):
    if not cond:
        raise AssertionError(msg)

check(data.get("ok") is True, "expected ok=true")
check(isinstance(data.get("reply"), str), "expected reply string")
check(isinstance(data.get("tool_calls"), list), "expected tool_calls list")
check(isinstance(data.get("context_store"), dict), "expected context_store dict")
ctx = data["context_store"]
check("driver_profile" in ctx, "missing context_store.driver_profile")
check("trip_context" in ctx, "missing context_store.trip_context")
check("last_actions" in ctx, "missing context_store.last_actions")
if label == "service_due":
    check(any(t.get("name") == "maintenance.suggest" for t in data["tool_calls"]),
          "service_due expected maintenance.suggest tool")
if label == "charging":
    check(any(t.get("name") == "places.search" for t in data["tool_calls"]),
          "charging expected places.search tool")
if label == "proposal":
    check(any(t.get("name") == "vehicle.set_state_proposal" for t in data["tool_calls"]),
          "proposal expected vehicle.set_state_proposal tool")
    check("proposal_action" in data, "proposal missing proposal_action")
    check("proposal_signature" in data, "proposal missing proposal_signature")
print(f"PASS: {label}")
PY

  rm -f "${tmp}"
}

run_case "service due reminder please" "service_due"
run_case "charging stop planning for route" "charging"
run_case "proposal heat 24" "proposal"

echo "All cloud contract checks passed."
