# Mock GenEd API Contracts

All payloads use JSON unless stated otherwise. The mock implementation is in
`tools/mock_server.py`.

## Authentication

Device requests reserve `X-Device-ID` and `X-Device-Token` headers. The current
mock server parses these headers but does not yet enforce token matching on
every route.

## POST /provision

Request:

```json
{"device_id": "dev_123"}
```

Success response:

```json
{"status": "ok", "device_id": "dev_123", "token": "token_value"}
```

## POST /telemetry

Accepts one `device_event.v1` envelope. A successful response acknowledges the
event ID:

```json
{"status": "ok", "event_id": "1", "ack": "committed"}
```

## POST /telemetry/batch

The response lists only committed IDs:

```json
{"status": "partial_ok", "acknowledged": ["1"]}
```

Unlisted IDs remain pending and may be resent.

## POST /ota/check

Accepts the current version. It returns `no_update` or metadata containing
version, size, chunk size, signature, SHA-256 and minimum battery.

## GET /ota/download?chunk=N

Returns one binary firmware chunk. The host OTA model does not yet consume
this endpoint.

## POST /diagnostics

Accepts a compact device-health and trace payload and returns
`{"status":"ok"}`.

