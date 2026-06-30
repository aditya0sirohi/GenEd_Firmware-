#!/usr/bin/env python3
"""
GenEd Mock Cloud Server
Firmware ke liye fake endpoints — provisioning, telemetry, OTA
Run: python tools/mock_server.py
"""

from flask import Flask, request, jsonify
import json
import time
import os

app = Flask(__name__)

# ============================================================
# In-memory state
# ============================================================
registered_devices = {}   # device_id → token
received_events    = []   # sab events yahan store honge
received_event_keys = set()  # (device_id, event_id) for idempotency
ota_available      = True # OTA update available hai?
current_fw_version = "0.1.0"
next_fw_version    = "0.2.0"

# ============================================================
# Auth helper
# ============================================================
def check_auth(req):
    device_id    = req.headers.get("X-Device-ID")
    device_token = req.headers.get("X-Device-Token")
    if not device_id or not device_token:
        return None, jsonify({"error": "missing auth"}), 401
    return device_id, None, None

# ============================================================
# 1. Provisioning endpoint
# ============================================================
@app.route("/provision", methods=["POST"])
def provision():
    data      = request.get_json(force=True)
    device_id = data.get("device_id", f"dev_{int(time.time())}")
    token     = f"token_{device_id}_{int(time.time())}"

    registered_devices[device_id] = token

    print(f"[PROVISION] registered device_id={device_id}")

    return jsonify({
        "status":    "ok",
        "device_id": device_id,
        "token":     token
    }), 200

# ============================================================
# 2. Telemetry endpoint — events receive karo
# ============================================================
@app.route("/telemetry", methods=["POST"])
def telemetry():
    data = request.get_json(force=True)

    event_id = data.get("event_id")
    seq      = data.get("sequence_number")
    etype    = data.get("event_type", "unknown")
    dev_id   = data.get("device_id", "unknown")

    key = (dev_id, str(event_id))
    duplicate = key in received_event_keys
    if not duplicate:
        received_events.append(data)
        received_event_keys.add(key)

    print(f"[TELEMETRY] received event_id={event_id}"
          f" seq={seq} type={etype} device={dev_id}"
          f" total_received={len(received_events)}")

    return jsonify({
        "status":   "ok",
        "event_id": event_id,
        "ack":      "committed",
        "duplicate": duplicate
    }), 200

# ============================================================
# 3. Telemetry — batch upload (partial ack simulation)
# ============================================================
@app.route("/telemetry/batch", methods=["POST"])
def telemetry_batch():
    data   = request.get_json(force=True)
    events = data.get("events", [])

    acknowledged = []
    for i, event in enumerate(events):
        # Simulate partial ack — reject last event sometimes
        if i == len(events) - 1 and len(events) > 2:
            print(f"[TELEMETRY_BATCH] simulating partial ack"
                  f" — rejecting last event")
            break
        key = (event.get("device_id", "unknown"),
               str(event.get("event_id")))
        if key not in received_event_keys:
            received_events.append(event)
            received_event_keys.add(key)
        acknowledged.append(event.get("event_id"))
        print(f"[TELEMETRY_BATCH] ack event_id="
              f"{event.get('event_id')}")

    return jsonify({
        "status":       "partial_ok",
        "acknowledged": acknowledged
    }), 200

# ============================================================
# 4. OTA check — update available hai?
# ============================================================
@app.route("/ota/check", methods=["POST"])
def ota_check():
    data       = request.get_json(force=True)
    current_v  = data.get("version", "0.0.0")

    print(f"[OTA_CHECK] device version={current_v}"
          f" available={ota_available}")

    if not ota_available or current_v == next_fw_version:
        return jsonify({"status": "no_update"}), 200

    return jsonify({
        "status":        "update_available",
        "version":       next_fw_version,
        "size_bytes":    524288,
        "chunk_size":    4096,
        "total_chunks":  128,
        "signature":     "fake_ed25519_signature_abc123",
        "sha256":        "fake_sha256_hash_xyz789",
        "min_battery":   30
    }), 200

# ============================================================
# 5. OTA download — chunks bhejo
# ============================================================
@app.route("/ota/download", methods=["GET"])
def ota_download():
    chunk_id = request.args.get("chunk", 0, type=int)
    total    = 128

    print(f"[OTA_DOWNLOAD] sending chunk {chunk_id}/{total}")

    # Fake chunk data
    chunk_data = bytes([chunk_id % 256] * 4096)

    return chunk_data, 200, {
        "Content-Type":  "application/octet-stream",
        "X-Chunk-ID":    str(chunk_id),
        "X-Total-Chunks": str(total)
    }

# ============================================================
# 6. Diagnostics export receive karo
# ============================================================
@app.route("/diagnostics", methods=["POST"])
def diagnostics():
    data = request.get_json(force=True)
    print(f"[DIAGNOSTICS] received export:"
          f" uptime={data.get('uptime_ms')}ms"
          f" battery={data.get('battery_pct')}%"
          f" pending={data.get('pending_events')}")
    return jsonify({"status": "ok"}), 200

# ============================================================
# 7. Debug endpoints — simulation control ke liye
# ============================================================
@app.route("/debug/events", methods=["GET"])
def debug_events():
    """Dekho kitne events receive hue"""
    return jsonify({
        "total":  len(received_events),
        "events": received_events[-10:]  # last 10
    }), 200

@app.route("/debug/ota/toggle", methods=["POST"])
def debug_ota_toggle():
    """OTA on/off karo"""
    global ota_available
    ota_available = not ota_available
    print(f"[DEBUG] OTA available = {ota_available}")
    return jsonify({"ota_available": ota_available}), 200

@app.route("/debug/reset", methods=["POST"])
def debug_reset():
    """Sab state reset karo"""
    global received_events, received_event_keys, registered_devices
    received_events    = []
    received_event_keys = set()
    registered_devices = {}
    print("[DEBUG] server state reset")
    return jsonify({"status": "reset"}), 200

# ============================================================
# Main
# ============================================================
if __name__ == "__main__":
    print("=" * 50)
    print("GenEd Mock Cloud Server")
    print("Endpoints:")
    print("  POST /provision")
    print("  POST /telemetry")
    print("  POST /telemetry/batch")
    print("  POST /ota/check")
    print("  GET  /ota/download?chunk=N")
    print("  POST /diagnostics")
    print("  GET  /debug/events")
    print("  POST /debug/ota/toggle")
    print("  POST /debug/reset")
    print("=" * 50)
    app.run(host="0.0.0.0", port=5000, debug=True)
