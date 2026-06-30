# Specification Decisions and Remaining Gaps

This document separates two different things:

1. Decisions required because the specification intentionally leaves a value
   or policy open.
2. Required behavior that is not yet implemented.

Documenting a decision does not mean the corresponding production feature is
complete.

## Decisions made for unspecified requirements

| Topic | Decision | Reason |
|---|---|---|
| OTA signature format | Model a 64-byte Ed25519-style signature | Small keys and signatures suit ESP32-class devices. The host crypto remains a deterministic fake. |
| Rules threshold | Repeated help is 3 requests within 5 minutes | Easy to configure later and deterministic in tests. |
| Engagement threshold | 10 minutes without activity | Matches the documented power idle interval. |
| Boot stabilization window | 30 seconds | Allows slow network initialization while remaining bounded. |
| Queue backpressure | Reject new events when the 100-event RAM queue is full | Never overwrite an older unacknowledged event. |
| Host event encoding | Escaped pipe-delimited records | Keeps the prototype dependency-free and readable. Production should use a length-delimited format such as CBOR. |

## Implemented and tested host behavior

- Stable event IDs and sequence numbers are assigned before durable append.
- A failed durable append does not enter the RAM queue.
- Acknowledgements are persisted before an event is marked committed in RAM.
- Acknowledged events are not replayed during recovery.
- Recovery is idempotent and preserves correlation IDs and payloads.
- Partial acknowledgement leaves only unacknowledged IDs pending.
- Simulated storage has bounded blob, log, record-count and record-size limits.
- Connectivity loss/recovery, flash-write failure, low battery and bad-signature
  scenarios have deterministic executable checks.
- Rules-engine derivation, power transitions, diagnostics traces/crash storage,
  and the basic host OTA slot model have unit checks.

## Remaining implementation gaps

### ESP32 target

The ESP32 HAL classes are declarations only. There is no ESP-IDF component
tree or verified ESP32 build. The CMake project currently builds the host
model only.

### Real host HTTP integration

`SimNetwork` deterministically models HTTP results but does not open a socket
to `mock_server.py`. The Flask server documents and exercises endpoint
contracts independently. A future `CurlNetwork` or equivalent host HAL is
needed for a real end-to-end HTTP integration test.

### Flash persistence and interrupted compaction

`SimStorage` is bounded but remains process memory. It models logical reboot
by constructing a new queue over the same storage instance; it does not
survive process termination. Interrupted append sectors, checksums, redundant
metadata and restart-safe compaction are not yet modeled.

### OTA completeness

The host model writes an inactive image, verifies through `ISecurity`, records
a pending slot and requires confirmation before changing the active slot.
It does not yet implement:

- signed metadata parsing;
- real chunk transfer and per-chunk hashes;
- resumable download offsets;
- anti-rollback version persistence;
- power-loss injection at every OTA phase;
- automatic watchdog-driven confirmation or rollback.

### Diagnostics completeness

The host implementation has a bounded trace ring, structured error storage,
crash storage and JSON trace export. ESP32 heap/stack watermarks, reset
metadata, per-task watchdog evidence, correlation IDs on every diagnostic and
automatic telemetry export remain incomplete.

### RTOS parity

The host application uses six `std::thread` tasks and mutex/atomic state. It
does not yet mirror FreeRTOS queues, event groups and task notifications, and
there is no ESP32 task implementation.

### Power and UI completeness

Core power-state selection is implemented. Adaptive sync batching, sleep
coordination, debounce timing, stuck-button handling and long-press timing are
not complete.

### Test and measurement coverage

The repository now contains executable host unit and scenario tests, but it
still lacks:

- process-restart persistence tests;
- interrupted compaction and OTA phase tests;
- long-duration/high-event-rate tests;
- CI configuration;
- measured CPU, stack, heap and flash-lifespan results on ESP-IDF.

## Readiness statement

The repository is a tested host-side foundation prototype. It is not yet an
ESP32 production foundation and should not be described as production-ready.
