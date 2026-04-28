# Failure Mode Analysis

This document captures failure-injection behavior for CabinOS and whether the current implementation matches expected behavior.

## Automated scenarios (executed via `scripts/run_failure_suite.sh`)

| Scenario | Injection Method | Expected Behavior | Observed Behavior | Status |
|---|---|---|---|---|
| Cloud unavailable (connection failure) | `scripts/failures/cloud_down.sh` calls unreachable `127.0.0.1:59999` | Connection fails quickly; edge should use offline path when configured offline | `curl` failed with connection error (`Couldn't connect to server`) | PASS |
| Malformed JSON payload | `scripts/failures/malformed_payload.sh` sends broken JSON body | Cloud bridge returns HTTP `400` and `error=invalid_json` | Received HTTP `400` with `invalid_json` | PASS |
| Missing utterance | `scripts/failures/missing_utterance.sh` omits `utterance` field | Cloud bridge returns HTTP `400` and `error=missing_utterance` | Received HTTP `400` with `missing_utterance` | PASS |

## Manual edge validation scenarios

These require interactive CLI and/or a mock proposal endpoint. Steps are documented in:

- `scripts/failures/edge_proposal_manual.md`

| Scenario | Injection Method | Expected Behavior | Status |
|---|---|---|---|
| Invalid signature | Set wrong `CABINOS_PROPOSAL_SECRET` and run `proposal heat 24` | `[proposal] rejected_invalid_signature` | PENDING |
| Replay nonce | Replay identical signed payload (same nonce/signature/timestamp) | `[proposal] rejected_replay_nonce` | PENDING |
| Stale timestamp | Use signed proposal older than 2 minutes | `[proposal] rejected_stale_timestamp` | PENDING |
| Out-of-bounds proposal value | Signed `set_temperature_c=99` | `[proposal] rejected_out_of_bounds_temperature` | PENDING |
| Action not allowlisted | Signed action such as `unlock_doors` | `[proposal] rejected_action_not_allowed` | PENDING |

## How to run

```bash
CABINOS_CLOUD_URL="https://<api-id>.execute-api.<region>.amazonaws.com/Prod/invoke" ./scripts/run_failure_suite.sh
```
