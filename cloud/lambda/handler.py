import json
import os
import time
import uuid
import hmac
import hashlib
import re
from typing import Any, Dict, List, Optional

import boto3
from botocore.exceptions import ClientError


def _ddb_resource():
    return boto3.resource("dynamodb")


def _upsert_session(
    session_id: str, utterance: str, reply: str, context_store: Dict[str, Any], tool_calls: List[Dict[str, Any]]
) -> Optional[str]:
    """Persist session metadata to DynamoDB. Returns None on success, else a short error string."""
    table_name = os.environ.get("SESSIONS_TABLE_NAME")
    if not table_name:
        return None
    table = _ddb_resource().Table(table_name)
    now_ms = int(time.time() * 1000)
    try:
        table.put_item(
            Item={
                "session_id": session_id,
                "last_utterance": utterance,
                "last_reply": reply,
                "driver_profile": context_store["driver_profile"],
                "trip_context": context_store["trip_context"],
                "last_actions": context_store["last_actions"],
                "last_tool_calls": tool_calls,
                "updated_at_ms": now_ms,
            }
        )
    except ClientError as exc:
        code = exc.response.get("Error", {}).get("Code", "")
        return f"dynamodb_put_item_failed:{code or 'unknown'}"
    except Exception as exc:  # noqa: BLE001 - never fail the HTTP response for persistence
        return f"dynamodb_put_item_failed:{exc!s}"
    return None


def _build_context_store(session_id: str, body: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "driver_profile": body.get(
            "driver_profile",
            {"driver_id": "default", "owner_since": "unknown", "preferred_temp_c": 22},
        ),
        "trip_context": body.get(
            "trip_context",
            {"session_id": session_id, "route_mode": "daily_commute", "soc_percent": 70},
        ),
        "last_actions": body.get("last_actions", []),
    }


def _tool_places_search(utterance: str) -> Optional[Dict[str, Any]]:
    text = utterance.lower()
    if "coffee" not in text and "charging" not in text:
        return None
    query = "coffee" if "coffee" in text else "ev charging"
    return {"name": "places.search", "arguments": {"query": query, "along_route": True}}


def _tool_vehicle_set_state_proposal(utterance: str) -> Optional[Dict[str, Any]]:
    text = utterance.lower()
    if "proposal heat" in text:
        return {"name": "vehicle.set_state_proposal", "arguments": {"action": "set_temperature_c"}}
    if "proposal dim" in text:
        return {"name": "vehicle.set_state_proposal", "arguments": {"action": "set_cabin_lights_percent"}}
    if "proposal hazards" in text:
        return {"name": "vehicle.set_state_proposal", "arguments": {"action": "set_hazards"}}
    return None


def _tool_maintenance_suggest(utterance: str) -> Optional[Dict[str, Any]]:
    text = utterance.lower()
    if "service due" in text or "maintenance" in text:
        return {"name": "maintenance.suggest", "arguments": {"category": "scheduled_maintenance"}}
    return None


def _build_tool_registry_calls(utterance: str) -> List[Dict[str, Any]]:
    tools: List[Dict[str, Any]] = []
    for fn in (_tool_places_search, _tool_vehicle_set_state_proposal, _tool_maintenance_suggest):
        t = fn(utterance)
        if t is not None:
            tools.append(t)
    return tools


def _stub_reply(utterance: str) -> str:
    text = utterance.lower()
    if "new owner setup" in text or "owner setup" in text:
        return (
            "New owner setup flow: profile initialized, climate preference saved, and onboarding checklist started."
        )
    if "service due" in text or "maintenance" in text:
        return "Service due flow: suggest booking within 7 days and propose preferred service center options."
    if "charging stop planning" in text or "plan charging stop" in text:
        return "Charging stop planning flow: suggested nearest high-power charger with minimal detour."
    if "coffee" in text:
        return (
            "Offline-capable suggestion: try the next rest area with a coffee shop. "
            "(Stub cloud bridge: replace with real OSM-backed search.)"
        )
    return "Stub cloud bridge reply: cognitive path online."


def _proposal_secret() -> str:
    return os.environ.get("SIGNED_CALLBACK_SECRET", "cabinos-dev-secret")


def _sign_proposal(action: str, value: int, timestamp_ms: int, nonce: str) -> str:
    canonical = f"{action}|{value}|{timestamp_ms}|{nonce}"
    return hmac.new(_proposal_secret().encode("utf-8"), canonical.encode("utf-8"), hashlib.sha256).hexdigest()


def _build_signed_proposal(utterance: str) -> Optional[Dict[str, Any]]:
    text = utterance.lower()

    action = None
    value = None

    # Explicit demo commands that stay on cognitive path and trigger signed proposals:
    # - "proposal heat 24"
    # - "proposal dim 15"
    # - "proposal hazards on|off"
    m = re.search(r"proposal\\s+heat\\s+(-?\\d+)", text)
    if m:
        action = "set_temperature_c"
        value = int(m.group(1))

    m = re.search(r"proposal\\s+dim\\s+(-?\\d+)", text)
    if m:
        action = "set_cabin_lights_percent"
        value = int(m.group(1))

    if "proposal hazards on" in text:
        action = "set_hazards"
        value = 1
    if "proposal hazards off" in text:
        action = "set_hazards"
        value = 0

    if action is None or value is None:
        return None

    timestamp_ms = int(time.time() * 1000)
    nonce = str(uuid.uuid4())
    signature = _sign_proposal(action, value, timestamp_ms, nonce)

    return {
        "proposal_action": action,
        "proposal_value": value,
        "proposal_timestamp_ms": timestamp_ms,
        "proposal_nonce": nonce,
        "proposal_signature": signature,
    }


def _env_flag_enabled(*keys: str) -> bool:
    for key in keys:
        value = os.environ.get(key, "").strip().lower()
        if value in {"1", "true", "yes", "on"}:
            return True
    return False


def _bedrock_model_id() -> str:
    return (os.environ.get("BEDROCK_MODEL_ID", "").strip() or os.environ.get("CLOUD_MODEL_ID", "").strip())

def _float_env(key: str, default: float) -> float:
    try:
        return float(os.environ.get(key, str(default)))
    except ValueError:
        return default

def _estimate_token_usage(utterance: str, reply: str) -> Dict[str, int]:
    # Lightweight heuristic used for cost observability without model-specific tokenizers.
    estimated_input_tokens = max(8, len(utterance) // 4)
    estimated_output_tokens = max(8, len(reply) // 4)
    return {
        "estimated_input_tokens": estimated_input_tokens,
        "estimated_output_tokens": estimated_output_tokens,
        "estimated_total_tokens": estimated_input_tokens + estimated_output_tokens,
    }

def _estimate_cost_usd(estimated_input_tokens: int, estimated_output_tokens: int) -> float:
    # Per-1k token rates are configurable so this stays model/vendor-agnostic in code.
    in_per_1k = _float_env("COST_PER_1K_INPUT_TOKENS_USD", 0.0008)
    out_per_1k = _float_env("COST_PER_1K_OUTPUT_TOKENS_USD", 0.0012)
    cost = (estimated_input_tokens / 1000.0) * in_per_1k + (estimated_output_tokens / 1000.0) * out_per_1k
    return round(cost, 8)

def _maybe_bedrock_reply(utterance: str) -> Optional[str]:
    if not _env_flag_enabled("USE_BEDROCK", "USE_CLOUD_MODEL"):
        return None

    model_id = _bedrock_model_id()
    if not model_id:
        return None

    client = boto3.client("bedrock-runtime")
    try:
        resp = client.converse(
            modelId=model_id,
            messages=[
                {
                    "role": "user",
                    "content": [{"text": utterance}],
                }
            ],
            inferenceConfig={"maxTokens": 256, "temperature": 0.2},
        )
        parts = resp["output"]["message"]["content"]
        if not parts:
            return None
        return parts[0].get("text")
    except Exception as exc:  # noqa: BLE001 - demo bridge should degrade gracefully
        return f"Bedrock inference failed, falling back to stub. Error: {exc!s}"


def lambda_handler(event, context):
    _ = context

    if event.get("httpMethod") == "GET":
        return {"statusCode": 200, "body": json.dumps({"ok": True, "service": "cabinos-cloud-bridge"})}

    if event.get("httpMethod") != "POST":
        return {"statusCode": 405, "body": json.dumps({"ok": False, "error": "method_not_allowed"})}

    try:
        body = json.loads(event.get("body") or "{}")
    except json.JSONDecodeError:
        return {"statusCode": 400, "body": json.dumps({"ok": False, "error": "invalid_json"})}

    session_id = str(body.get("session_id") or "").strip()
    utterance = str(body.get("utterance") or "").strip()
    if not utterance:
        return {"statusCode": 400, "body": json.dumps({"ok": False, "error": "missing_utterance"})}
    if not session_id:
        session_id = str(uuid.uuid4())

    context_store = _build_context_store(session_id, body)
    tool_calls = _build_tool_registry_calls(utterance)

    reply = _maybe_bedrock_reply(utterance)
    if reply is None:
        reply = _stub_reply(utterance)

    context_store["last_actions"] = (context_store.get("last_actions", []) + [{"utterance": utterance, "reply": reply}])[-10:]
    ddb_error = _upsert_session(session_id, utterance, reply, context_store, tool_calls)

    payload = {
        "ok": True,
        "session_id": session_id,
        "reply": reply,
        "tool_calls": tool_calls,
        "context_store": context_store,
        "tokens_estimate": max(16, min(512, len(utterance) * 2)),
    }
    token_usage = _estimate_token_usage(utterance, reply)
    payload.update(token_usage)
    payload["estimated_cost_usd"] = _estimate_cost_usd(
        token_usage["estimated_input_tokens"], token_usage["estimated_output_tokens"]
    )

    proposal = _build_signed_proposal(utterance)
    if proposal is not None:
        payload.update(proposal)

    if ddb_error:
        # SAM local often runs before the stack (and DynamoDB table) exists; still return a valid reply.
        payload["ddb_persisted"] = False
        payload["ddb_error"] = ddb_error
    else:
        payload["ddb_persisted"] = True
    return {"statusCode": 200, "body": json.dumps(payload)}
