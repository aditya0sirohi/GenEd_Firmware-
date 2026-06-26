# State Machine: Connectivity

## States

```
DISCONNECTED → CONNECTING → CONNECTED → DEGRADED
```

## Diagram

```
         boot / wifi init
                │
                ▼
        ┌──────────────┐
        │ DISCONNECTED │◄────────────────────────┐
        └──────┬───────┘                         │
               │ connect() called                │
               ▼                                 │
        ┌────────────┐                           │
        │ CONNECTING │                           │
        └──────┬─────┘                           │
               │                                 │
        ┌──────┴──────────────┐                  │
        │                     │                  │
        │ success             │ timeout/fail      │
        ▼                     ▼                  │
   ┌─────────┐         ┌──────────────┐          │
   │CONNECTED│         │ DISCONNECTED │          │
   └────┬────┘         └──────────────┘          │
        │                                        │
        │ signal weak / partial loss             │
        ▼                                        │
   ┌──────────┐                                  │
   │ DEGRADED │                                  │
   └────┬─────┘                                  │
        │                                        │
        ├── signal restored ──► CONNECTED        │
        │                                        │
        └── full loss ────────────────────────────┘

```

## Transitions

| From | To | Guard | Action |
|------|----|-------|--------|
| DISCONNECTED | CONNECTING | connect() called | start WiFi |
| CONNECTING | CONNECTED | SSID joined | emit connectivity_restored |
| CONNECTING | DISCONNECTED | timeout after 3 retries | emit connectivity_lost |
| CONNECTED | DEGRADED | RSSI < -80 dBm | throttle sync |
| CONNECTED | DISCONNECTED | AP lost | emit connectivity_lost |
| DEGRADED | CONNECTED | RSSI > -70 dBm | resume normal sync |
| DEGRADED | DISCONNECTED | full signal loss | emit connectivity_lost |

## Retry Policy

```
Attempt 1: immediate
Attempt 2: 2 seconds
Attempt 3: 4 seconds
Attempt 4+: 30 seconds (exponential backoff capped)
```

## Effect on Other Tasks

| State | SyncTask | OtaTask |
|-------|----------|---------|
| CONNECTED | uploads normally | checks updates |
| DEGRADED | reduced batch size | paused |
| DISCONNECTED | buffers events | paused |
| CONNECTING | buffers events | paused |

## Diagnostics

```
[CONNECTIVITY] CONNECTING → CONNECTED rssi=-58dBm
[CONNECTIVITY] CONNECTED → DISCONNECTED reason=ap_lost
[CONNECTIVITY] DISCONNECTED → CONNECTING attempt=2 backoff=2000ms
```
```