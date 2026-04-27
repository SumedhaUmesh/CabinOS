#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://127.0.0.1:3000}"

echo "GET ${BASE_URL}/"
curl -fsS "${BASE_URL}/" | python3 -m json.tool

echo
echo "POST ${BASE_URL}/invoke"
curl -fsS -X POST "${BASE_URL}/invoke" \
  -H 'Content-Type: application/json' \
  -d '{"session_id":"smoke-session","utterance":"find coffee on my route"}' \
  | python3 -m json.tool
