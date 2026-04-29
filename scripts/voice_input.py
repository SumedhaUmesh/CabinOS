#!/usr/bin/env python3
"""
CabinOS voice input pipeline — Phase 1.

Transcribes audio (mic or file) and routes the utterance through the
CabinOS cloud bridge, mirroring the same tier classification logic used
by the C++ PolicyEngine.

Modes
-----
  --text  "utterance"     skip ASR; send raw text (use this for testing)
  --file  path/to/audio   transcribe a WAV/MP3/M4A file with Whisper
  --live  [--duration N]  record N seconds from the default mic (default 5s)

Environment
-----------
  CABINOS_CLOUD_URL   POST target (default: http://127.0.0.1:3000/invoke)
  CABINOS_SESSION_ID  session identifier (default: voice-session)

Examples
--------
  # Test without audio hardware:
  python3 scripts/voice_input.py --text "find coffee on my route"

  # Transcribe a file:
  python3 scripts/voice_input.py --file scripts/test_audio/sample.wav

  # Live mic (5s default):
  python3 scripts/voice_input.py --live --duration 5
"""

import argparse
import json
import os
import sys
import tempfile
import time
import uuid

# ---------------------------------------------------------------------------
# Optional heavy deps — imported lazily inside functions so --text mode
# starts instantly without touching torch/whisper/sounddevice.
# ---------------------------------------------------------------------------

try:
    import requests as _requests_mod
    _HAS_REQUESTS = True
except ImportError:
    _HAS_REQUESTS = False


# ---------------------------------------------------------------------------
# Tier classification — mirrors C++ PolicyEngine keyword logic
# ---------------------------------------------------------------------------

_SAFETY_KEYWORDS = {"hazard", "defog", "brake"}
_COMFORT_KEYWORDS = {"cabin", "temperature", "light", "battery", "soc", "charge"}


def classify_tier(utterance: str) -> str:
    text = utterance.lower()
    for kw in _SAFETY_KEYWORDS:
        if kw in text:
            return "safety_critical"
    for kw in _COMFORT_KEYWORDS:
        if kw in text:
            return "comfort"
    return "cognitive"


# ---------------------------------------------------------------------------
# ASR
# ---------------------------------------------------------------------------

_whisper_model_cache = {}


def _load_whisper(model_size: str = "tiny"):
    try:
        import whisper as _whisper_mod
    except ImportError:
        print("[asr] openai-whisper not installed. Install with:")
        print("      pip install openai-whisper")
        sys.exit(1)
    if model_size not in _whisper_model_cache:
        print(f"[asr] loading Whisper '{model_size}' model (first run downloads ~75 MB)…")
        _whisper_model_cache[model_size] = _whisper_mod.load_model(model_size)
    return _whisper_model_cache[model_size]


def transcribe_file(path: str, model_size: str = "tiny") -> str:
    model = _load_whisper(model_size)
    print(f"[asr] transcribing '{path}'…")
    result = model.transcribe(path, language="en", fp16=False)
    return result["text"].strip()


def record_and_transcribe(duration: int, model_size: str = "tiny") -> str:
    try:
        import sounddevice as _sd
        import soundfile as _sf
    except ImportError:
        print("[asr] sounddevice/soundfile not installed. Install with:")
        print("      pip install sounddevice soundfile")
        sys.exit(1)
    samplerate = 16_000
    print(f"[mic] recording {duration}s — speak now…")
    audio = _sd.rec(
        int(duration * samplerate),
        samplerate=samplerate,
        channels=1,
        dtype="float32",
    )
    _sd.wait()
    print("[mic] recording done.")

    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        _sf.write(tmp_path, audio, samplerate)
        return transcribe_file(tmp_path, model_size)
    finally:
        try:
            os.unlink(tmp_path)
        except OSError:
            pass


# ---------------------------------------------------------------------------
# Cloud bridge
# ---------------------------------------------------------------------------

def _cloud_url() -> str:
    return os.environ.get("CABINOS_CLOUD_URL", "http://127.0.0.1:3000/invoke")


def _session_id() -> str:
    return os.environ.get("CABINOS_SESSION_ID", f"voice-{uuid.uuid4().hex[:8]}")


def invoke_cloud_bridge(utterance: str, session_id: str) -> dict:
    if not _HAS_REQUESTS:
        print("[cloud] 'requests' not installed. Install with: pip install requests")
        sys.exit(1)
    url = _cloud_url()
    payload = {"session_id": session_id, "utterance": utterance}
    try:
        resp = _requests_mod.post(url, json=payload, timeout=10)
        resp.raise_for_status()
        return resp.json()
    except _requests_mod.exceptions.ConnectionError:
        return {"ok": False, "error": f"cloud_bridge_unreachable (is it running at {url}?)"}
    except _requests_mod.exceptions.Timeout:
        return {"ok": False, "error": "cloud_bridge_timeout"}
    except Exception as exc:
        return {"ok": False, "error": str(exc)}


# ---------------------------------------------------------------------------
# Display
# ---------------------------------------------------------------------------

_TIER_COLORS = {
    "safety_critical": "\033[91m",  # red
    "comfort":         "\033[93m",  # yellow
    "cognitive":       "\033[94m",  # blue
}
_RESET = "\033[0m"


def _tier_label(tier: str) -> str:
    color = _TIER_COLORS.get(tier, "")
    return f"{color}[{tier}]{_RESET}"


def print_result(utterance: str, tier: str, cloud_result: dict | None, cloud_skip_reason: str = "") -> None:
    print()
    print(f"  utterance : {utterance}")
    print(f"  tier      : {_tier_label(tier)}")
    if cloud_result is None:
        reason = cloud_skip_reason or ("edge-only by policy" if tier == "safety_critical" else "--no-cloud flag")
        print(f"  cloud     : skipped ({reason})")
        return
    if not cloud_result.get("ok"):
        print(f"  cloud     : ERROR — {cloud_result.get('error', 'unknown')}")
        return
    print(f"  reply     : {cloud_result.get('reply', '(no reply)')}")
    if cloud_result.get("tool_calls"):
        for tc in cloud_result["tool_calls"]:
            print(f"  tool_call : {tc.get('name')} {tc.get('arguments', {})}")
    if cloud_result.get("estimated_cost_usd") is not None:
        cost = cloud_result["estimated_cost_usd"]
        tokens = cloud_result.get("estimated_total_tokens", "?")
        print(f"  cost      : ${cost:.6f}  ({tokens} tokens est.)")
    if cloud_result.get("has_proposal") or cloud_result.get("proposal_action"):
        print(f"  proposal  : {cloud_result.get('proposal_action')} = {cloud_result.get('proposal_value')}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="CabinOS voice input — Phase 1 ASR pipeline",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--text", metavar="UTTERANCE", help="skip ASR; send raw text")
    group.add_argument("--file", metavar="PATH", help="transcribe an audio file")
    group.add_argument("--live", action="store_true", help="record from the default mic")

    parser.add_argument("--duration", type=int, default=5, metavar="SECONDS",
                        help="mic recording duration (--live mode, default 5)")
    parser.add_argument("--model", default="tiny", metavar="SIZE",
                        help="Whisper model size: tiny/base/small/medium/large (default: tiny)")
    parser.add_argument("--no-cloud", action="store_true",
                        help="classify tier only; do not POST to the cloud bridge")
    args = parser.parse_args()

    # --- Transcribe or pass through ---
    t0 = time.monotonic()
    if args.text:
        utterance = args.text.strip()
        print(f"[input] text mode: '{utterance}'")
    elif args.file:
        utterance = transcribe_file(args.file, args.model)
        print(f"[asr] transcript: '{utterance}'")
    else:
        utterance = record_and_transcribe(args.duration, args.model)
        print(f"[asr] transcript: '{utterance}'")

    if not utterance:
        print("[error] empty utterance — nothing to route.")
        sys.exit(1)

    # --- Classify tier ---
    tier = classify_tier(utterance)

    # --- Route ---
    session_id = _session_id()
    cloud_result = None
    cloud_skip_reason = ""

    if args.no_cloud:
        cloud_skip_reason = "--no-cloud flag"
    elif tier == "safety_critical":
        cloud_skip_reason = "edge-only by policy"
        print(f"[router] safety_critical → edge-only path (cloud skipped by policy)")
    else:
        print(f"[router] {tier} → invoking cloud bridge at {_cloud_url()}")
        cloud_result = invoke_cloud_bridge(utterance, session_id)

    elapsed = (time.monotonic() - t0) * 1000
    print_result(utterance, tier, cloud_result, cloud_skip_reason)
    print(f"\n  total elapsed: {elapsed:.1f} ms")


if __name__ == "__main__":
    main()
