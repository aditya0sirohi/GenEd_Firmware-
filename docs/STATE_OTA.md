# State Machine: OTA Update

## States

```
IDLE → METADATA_CHECK → DOWNLOADING → VERIFYING →
READY_TO_BOOT → PENDING_CONFIRM → CONFIRMED / ROLLBACK
```

## Diagram

```
        boot / 30s timer
               │
               ▼
          ┌─────────┐
     ┌───►│  IDLE   │◄─────────────────────────────┐
     │    └────┬────┘                               │
     │         │ connected + not low battery        │
     │         ▼                                    │
     │  ┌───────────────┐                           │
     │  │ METADATA_CHECK│                           │
     │  └───────┬───────┘                           │
     │          │                                   │
     │   ┌──────┴──────────┐                        │
     │   │                 │                        │
     │   │ no update       │ update available        │
     └───┘                 ▼                        │
                    ┌─────────────┐                 │
              ┌────►│ DOWNLOADING │                 │
              │     └──────┬──────┘                 │
              │            │                        │
              │     ┌──────┴────────────┐           │
              │     │                   │           │
              │     │ complete    power/network loss │
              │     ▼                   ▼           │
              │  ┌──────────┐    ┌─────────────┐   │
              │  │VERIFYING │    │ INTERRUPTED │   │
              │  └────┬─────┘    └──────┬──────┘   │
              │       │                 │           │
              │  ┌────┴──────┐   resume safe        │
              │  │           │         └────────────┘
              │  │sig valid  │ invalid image
              │  ▼           ▼
              │  ┌──────────────┐    ┌────────┐
              │  │READY_TO_BOOT │    │ FAILED │──► IDLE
              │  └──────┬───────┘    └────────┘
              │         │ reboot into slot B
              │         ▼
              │  ┌────────────────┐
              │  │ PENDING_CONFIRM│
              │  └───────┬────────┘
              │          │
              │   ┌──────┴──────────────┐
              │   │                     │
              │   │ health pass (30s)   │ crash/watchdog
              │   ▼                     ▼
              │  ┌───────────┐    ┌──────────┐
              │  │ CONFIRMED │    │ ROLLBACK │
              │  └───────────┘    └────┬─────┘
              │                        │ boot slot A
              └────────────────────────┘
```

## Transitions

| From | To | Guard | Action |
|------|----|-------|--------|
| IDLE | METADATA_CHECK | connected + battery > 30% | poll /ota/check |
| METADATA_CHECK | DOWNLOADING | update available | emit ota_started |
| METADATA_CHECK | IDLE | no update | wait 30s |
| DOWNLOADING | VERIFYING | all chunks received | verify hash |
| DOWNLOADING | INTERRUPTED | power/network loss | save chunk offset |
| INTERRUPTED | DOWNLOADING | connectivity restored | resume from offset |
| VERIFYING | READY_TO_BOOT | signature + hash valid | prepare slot B |
| VERIFYING | FAILED | invalid signature/hash | emit ota_failed |
| READY_TO_BOOT | PENDING_CONFIRM | reboot into slot B | run health checks |
| PENDING_CONFIRM | CONFIRMED | all tasks healthy 30s | commit slot B |
| PENDING_CONFIRM | ROLLBACK | crash/watchdog timeout | boot slot A |
| ROLLBACK | IDLE | slot A active | emit ota_failed |
| FAILED | IDLE | reset | wait next cycle |

## Boot Confirmation Criteria

All must pass within 30 second window:
```
✓ IStorage mounted + queue recovered
✓ INetwork initialized (or bounded failure)
✓ DiagnosticsTask operational
✓ All 6 tasks kicking watchdog
```

## Anti-Rollback Policy

```
Current min version stored in flash: "min_fw_version"
Incoming version < min version → REJECT in METADATA_CHECK
Never downgrade below last confirmed version
```

## Power Policy

```
battery < 30% → stay in IDLE, skip check
battery drops during DOWNLOADING → INTERRUPTED (safe resume)
brownout during slot write → chunk re-downloaded on recovery
```

## Diagnostics

```
[OTA] IDLE → METADATA_CHECK
[OTA] METADATA_CHECK → DOWNLOADING version=0.2.0 size=524288
[OTA] DOWNLOADING chunk=45/128
[OTA] DOWNLOADING → INTERRUPTED reason=power_loss chunk=45
[OTA] INTERRUPTED → DOWNLOADING resume_chunk=45
[OTA] VERIFYING → READY_TO_BOOT signature=valid
[OTA] PENDING_CONFIRM → CONFIRMED uptime=31s tasks_healthy=6
[OTA] PENDING_CONFIRM → ROLLBACK reason=watchdog_timeout
```
```