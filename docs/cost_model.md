# Cost Model

CabinOS exposes lightweight token and cost estimates in cloud responses for cognitive requests.

## Response fields

- `estimated_input_tokens`
- `estimated_output_tokens`
- `estimated_total_tokens`
- `estimated_cost_usd`

## Estimation formula

Token heuristic in `cloud/lambda/handler.py`:

- `estimated_input_tokens = max(8, len(utterance) / 4)`
- `estimated_output_tokens = max(8, len(reply) / 4)`

Cost formula:

- `estimated_cost_usd = (input_tokens / 1000) * input_rate + (output_tokens / 1000) * output_rate`

Environment-configurable rates:

- `COST_PER_1K_INPUT_TOKENS_USD` (default `0.0008`)
- `COST_PER_1K_OUTPUT_TOKENS_USD` (default `0.0012`)

## Per-1000 request estimate

For an average request where:

- input tokens = `40`
- output tokens = `120`

Cost per request:

- `(40 / 1000 * 0.0008) + (120 / 1000 * 0.0012) = 0.000176 USD`

Cost per 1,000 requests:

- `0.176 USD`

## Notes

- These are observability estimates, not bill-perfect token accounting.
- Final pricing depends on selected model and region.
- Keep rates synchronized with current Bedrock pricing for your chosen model.
