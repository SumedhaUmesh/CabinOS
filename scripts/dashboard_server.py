#!/usr/bin/env python3
"""
CabinOS web dashboard server.

Serves the HMI dashboard and proxies utterances through the same tier
classification and cloud bridge used by the CLI voice pipeline.

Usage
-----
  source .venv/bin/activate
  export CABINOS_CLOUD_URL="http://127.0.0.1:4000/invoke"
  python3 scripts/dashboard_server.py          # opens on http://127.0.0.1:8080

Options (env vars)
------------------
  CABINOS_CLOUD_URL     cloud bridge endpoint (default: http://127.0.0.1:3000/invoke)
  CABINOS_SESSION_ID    session id (default: dashboard-session)
  DASHBOARD_PORT        HTTP port for this server (default: 8080)
"""

import os
import re
import uuid
from pathlib import Path

import requests
from fastapi import FastAPI
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

app = FastAPI(title="CabinOS Dashboard")

# ---------------------------------------------------------------------------
# Simulated cabin state (updated by comfort/safety commands)
# ---------------------------------------------------------------------------

cabin_state: dict = {
    "temperature_c": 22,
    "lights_percent": 50,
    "hazards_on": False,
    "battery_soc": 70,
}

# ---------------------------------------------------------------------------
# Tier classification — mirrors C++ PolicyEngine
# ---------------------------------------------------------------------------

_SAFETY_KEYWORDS = {"hazard", "defog", "brake"}
_COMFORT_KEYWORDS = {"cabin", "temperature", "temp", "light", "dim", "battery", "soc", "charge"}


def classify_tier(utterance: str) -> str:
    text = utterance.lower()
    for kw in _SAFETY_KEYWORDS:
        if kw in text:
            return "safety_critical"
    for kw in _COMFORT_KEYWORDS:
        if kw in text:
            return "comfort"
    return "cognitive"


def _update_cabin_state(utterance: str, tier: str) -> None:
    text = utterance.lower()
    if tier == "comfort":
        if "temp" in text or "temperature" in text:
            m = re.search(r"(\d+)", text)
            if m:
                cabin_state["temperature_c"] = int(m.group(1))
        elif "light" in text or "dim" in text:
            m = re.search(r"(\d+)", text)
            if m:
                cabin_state["lights_percent"] = min(100, int(m.group(1)))
    elif tier == "safety_critical" and "hazard" in text:
        cabin_state["hazards_on"] = "off" not in text

# ---------------------------------------------------------------------------
# Cloud bridge
# ---------------------------------------------------------------------------

def _cloud_url() -> str:
    return os.environ.get("CABINOS_CLOUD_URL", "http://127.0.0.1:3000/invoke")


def _invoke_cloud(utterance: str, session_id: str) -> dict:
    try:
        resp = requests.post(
            _cloud_url(),
            json={"session_id": session_id, "utterance": utterance},
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ConnectionError:
        return {"ok": False, "error": f"cloud_bridge_unreachable ({_cloud_url()})"}
    except requests.exceptions.Timeout:
        return {"ok": False, "error": "cloud_bridge_timeout"}
    except Exception as exc:
        return {"ok": False, "error": str(exc)}

# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------

DASHBOARD_DIR = Path(__file__).parent / "dashboard"


@app.get("/")
def index():
    return FileResponse(DASHBOARD_DIR / "index.html")


@app.get("/api/state")
def get_state():
    return JSONResponse(cabin_state)


class InvokeRequest(BaseModel):
    utterance: str
    session_id: str = ""


@app.post("/api/invoke")
def invoke(body: InvokeRequest):
    utterance = body.utterance.strip()
    if not utterance:
        return JSONResponse({"ok": False, "error": "missing_utterance"}, status_code=400)

    session_id = body.session_id or os.environ.get("CABINOS_SESSION_ID", f"dashboard-{uuid.uuid4().hex[:8]}")
    tier = classify_tier(utterance)
    _update_cabin_state(utterance, tier)

    if tier == "safety_critical":
        return JSONResponse({
            "ok": True,
            "tier": tier,
            "reply": "Handled on-device — cloud bypassed by policy.",
            "tool_calls": [],
            "cloud_used": False,
            "cabin_state": cabin_state,
        })

    cloud = _invoke_cloud(utterance, session_id)
    if not cloud.get("ok"):
        return JSONResponse({
            "ok": False,
            "tier": tier,
            "error": cloud.get("error", "cloud_error"),
            "cloud_used": True,
            "cabin_state": cabin_state,
        })

    return JSONResponse({
        "ok": True,
        "tier": tier,
        "reply": cloud.get("reply", ""),
        "tool_calls": cloud.get("tool_calls", []),
        "cloud_used": True,
        "cabin_state": cabin_state,
        "estimated_cost_usd": cloud.get("estimated_cost_usd"),
        "estimated_total_tokens": cloud.get("estimated_total_tokens"),
    })


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import uvicorn
    port = int(os.environ.get("DASHBOARD_PORT", 8080))
    print(f"CabinOS dashboard → http://127.0.0.1:{port}")
    print(f"Cloud bridge      → {_cloud_url()}")
    uvicorn.run(app, host="127.0.0.1", port=port)
