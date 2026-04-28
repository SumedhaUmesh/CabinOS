#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <cloud_url>"
  exit 2
fi

CLOUD_URL="$1"

echo "Scenario: malformed_payload"
echo "Expected: HTTP 400 with error=invalid_json"

RAW_RESPONSE="$(curl -sS -w "\nHTTP_STATUS:%{http_code}" -X POST "${CLOUD_URL}" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"oops","utterance": }')"

HTTP_STATUS="$(printf "%s" "${RAW_RESPONSE}" | sed -n 's/.*HTTP_STATUS:\([0-9][0-9]*\).*/\1/p')"
BODY="$(printf "%s" "${RAW_RESPONSE}" | sed 's/HTTP_STATUS:.*$//')"

if [[ "${HTTP_STATUS}" != "400" ]]; then
  echo "FAIL: expected HTTP 400, got ${HTTP_STATUS}"
  echo "Body: ${BODY}"
  exit 1
fi

if ! printf "%s" "${BODY}" | python3 -c "import json,sys; d=json.load(sys.stdin); assert d.get('error')=='invalid_json'"; then
  echo "FAIL: expected error=invalid_json"
  echo "Body: ${BODY}"
  exit 1
fi

echo "PASS: malformed_payload returned HTTP 400 invalid_json."
