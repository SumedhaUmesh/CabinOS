# Edge Proposal Manual Failure Checks

These checks validate edge-side rejection paths for signed cloud proposals.

Prerequisites:

- `cabinos_cli` built and runnable.
- Cloud bridge endpoint reachable.
- `CABINOS_CLOUD_URL` set.
- `CABINOS_SESSION_ID` set.
- `CABINOS_PROPOSAL_SECRET` intentionally set for each case below.

## 1) Invalid signature

1. Set wrong edge secret:
   - `export CABINOS_PROPOSAL_SECRET=wrong-secret`
2. Run `./build/edge/cabinos_cli` and answer `y` for cloud online.
3. Enter: `proposal heat 24`
4. Expected suffix in response:
   - `[proposal] rejected_invalid_signature`

## 2) Replay nonce

1. Set correct secret:
   - `export CABINOS_PROPOSAL_SECRET=<same as SIGNED_CALLBACK_SECRET>`
2. Trigger one accepted proposal:
   - `proposal dim 20`
3. Replay exact same cloud response payload via a local mock endpoint (same nonce/signature/timestamp).
4. Expected:
   - `[proposal] rejected_replay_nonce`

## 3) Stale timestamp

1. Use a mock response where `proposal_timestamp_ms` is older than 2 minutes.
2. Expected:
   - `[proposal] rejected_stale_timestamp`

## 4) Out-of-bounds action values

1. Use proposal with:
   - `proposal_action=set_temperature_c`, `proposal_value=99`
2. Expected:
   - `[proposal] rejected_out_of_bounds_temperature`

## 5) Action not allowlisted

1. Use proposal with:
   - `proposal_action=unlock_doors`
2. Expected:
   - `[proposal] rejected_action_not_allowed`
