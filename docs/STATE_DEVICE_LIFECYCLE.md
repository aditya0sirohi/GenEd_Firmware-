# State Machine: Device Lifecycle

## States

```
BOOTING → PROVISIONING → ACTIVE → RECOVERY → SHUTDOWN
```

## Diagram

```
         power on
            │
            ▼
        ┌─────────┐
        │ BOOTING │
        └────┬────┘
             │ HAL init OK
             │ queue recovered
             ▼
      ┌──────────────┐
      │ PROVISIONING │◄─── no device_id in storage
      └──────┬───────┘
             │ device_id assigned
             │ token stored
             ▼
        ┌────────┐
   ┌───►│ ACTIVE │◄──────────────────┐
   │    └────┬───┘                   │
   │         │                       │
   │    ┌────┴──────────────┐        │
   │    │ failure conditions│        │
   │    └────┬──────────────┘        │
   │         │ repeated boot fails   │
   │         │ storage corrupt       │
   │         │ OTA rollback loop     │
   │         ▼                       │
   │    ┌──────────┐                 │
   │    │ RECOVERY │                 │
   │    └────┬─────┘                 │
   │         │ health restored       │
   └─────────┘                       │
                                     │
        ┌──────────┐                 │
        │ SHUTDOWN │                 │
        └──────────┘                 │
             ▲                       │
             │ graceful stop         │
             └───────────────────────┘
```

## Transitions

| From | To | Guard | Action |
|------|----|-------|--------|
| BOOTING | PROVISIONING | no device_id in flash | start provision flow |
| BOOTING | ACTIVE | device_id exists | start all tasks |
| ACTIVE | RECOVERY | 3+ consecutive boot failures | limit task set |
| ACTIVE | SHUTDOWN | shutdown signal | stop all tasks |
| RECOVERY | ACTIVE | health checks pass | resume normal |

## Invalid Transitions

- SHUTDOWN → any state (requires full reboot)
- PROVISIONING → RECOVERY (must complete provision first)

## Diagnostics

Every transition emits a trace entry:
```
[LIFECYCLE] BOOTING → ACTIVE reason=normal_boot
[LIFECYCLE] ACTIVE → RECOVERY reason=storage_corrupt
```
```
