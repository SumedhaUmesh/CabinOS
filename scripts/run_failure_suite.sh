#!/usr/bin/env bash
set -euo pipefail

CLOUD_URL="${CABINOS_CLOUD_URL:-}"

if [[ -z "${CLOUD_URL}" ]]; then
  echo "Usage: CABINOS_CLOUD_URL=<url> $0"
  echo "Example:"
  echo "  CABINOS_CLOUD_URL=\"https://.../Prod/invoke\" ./scripts/run_failure_suite.sh"
  exit 2
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "Running CabinOS automated failure suite..."
echo "Cloud URL: ${CLOUD_URL}"
echo

bash "${ROOT_DIR}/scripts/failures/cloud_down.sh" "${CLOUD_URL}"
echo
bash "${ROOT_DIR}/scripts/failures/malformed_payload.sh" "${CLOUD_URL}"
echo
bash "${ROOT_DIR}/scripts/failures/missing_utterance.sh" "${CLOUD_URL}"
echo

echo "Automated checks complete."
echo "Manual edge proposal checks:"
echo "  ${ROOT_DIR}/scripts/failures/edge_proposal_manual.md"
