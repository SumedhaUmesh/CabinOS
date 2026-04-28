#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <cloud_url>"
  exit 2
fi

CLOUD_URL="$1"

# Inject failure by intentionally targeting an unreachable host/port.
BROKEN_URL="http://127.0.0.1:59999/invoke"

echo "Scenario: cloud_down"
echo "Expected: request should fail quickly with connection error."
echo "Running: curl to ${BROKEN_URL}"

if curl -sS -m 2 -X POST "${BROKEN_URL}" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"failure-cloud-down","utterance":"find coffee on my route"}' >/dev/null; then
  echo "FAIL: unexpected success against broken URL"
  exit 1
fi

echo "PASS: cloud_down injection produced expected connection failure."
echo "Note: validate graceful fallback in cabinos_cli manually with cloud_online=n."
