import json
import os
import time
import uuid
from typing import Any, Dict, List, Optional

import boto3
from botocore.exceptions import ClientError


def _ddb_resource():
    return boto3.resource("dynamodb")


def _upsert_session(session_id: str, utterance: str, reply: str) -> Optional[str]:
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
                "updated_at_ms": now_ms,
            }
        )
    except ClientError as exc:
        code = exc.response.get("Error", {}).get("Code", "")
        return f"dynamodb_put_item_failed:{code or 'unknown'}"
    except Exception as exc:  # noqa: BLE001 - never fail the HTTP response for persistence
        return f"dynamodb_put_item_failed:{exc!s}"
    return None


def _stub_tool_calls(utterance: str) -> List[Dict[str, Any]]:
    text = utterance.lower()
    tools: List[Dict[str, Any]] = []
    if "coffee" in text:
        tools.append(
            {
                "name": "places.search",
                "arguments": {"query": "coffee", "along_route": True},
            }
        )
    return tools


def _stub_reply(utterance: str) -> str:
    text = utterance.lower()
    if "coffee" in text:
        return (
            "Offline-capable suggestion: try the next rest area with a coffee shop. "
            "(Stub cloud bridge: replace with real OSM-backed search.)"
        )
    return "Stub cloud bridge reply: cognitive path online."


def _env_flag_enabled(key: str) -> bool:
    value = os.environ.get(key, "").strip().lower()
    return value in {"1", "true", "yes", "on"}


def _cloud_model_id() -> str:
    return os.environ.get("CLOUD_MODEL_ID", "").strip()


def _maybe_managed_model_reply(utterance: str) -> Optional[str]:
    if not _env_flag_enabled("USE_CLOUD_MODEL"):
        return None

    model_id = _cloud_model_id()
    if not model_id:
        return None

    # Uses the AWS managed inference HTTP API for configured models in your account/region.
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
        return f"Managed model inference failed, falling back to stub. Error: {exc!s}"


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

    tool_calls = _stub_tool_calls(utterance)

    reply = _maybe_managed_model_reply(utterance)
    if reply is None:
        reply = _stub_reply(utterance)

    ddb_error = _upsert_session(session_id, utterance, reply)

    payload = {
        "ok": True,
        "session_id": session_id,
        "reply": reply,
        "tool_calls": tool_calls,
        "tokens_estimate": max(16, min(512, len(utterance) * 2)),
    }
    if ddb_error:
        # SAM local often runs before the stack (and DynamoDB table) exists; still return a valid reply.
        payload["ddb_persisted"] = False
        payload["ddb_error"] = ddb_error
    else:
        payload["ddb_persisted"] = True
    return {"statusCode": 200, "body": json.dumps(payload)}
