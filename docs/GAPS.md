# GAPS.md — Specification Gaps & Assumptions

This document identifies intentional and unintentional gaps in the
specification, the assumptions made, and the rationale behind each
decision. As noted in the assignment brief: "The spec has some
intentional gaps. Part of the evaluation involves identifying these
gaps and moving in the correct direction rather than making silent
assumptions."

---

## GAP 1: OTA Signing Algorithm

**What the spec says:** "Signed update metadata and signed firmware
image verification" — no algorithm specified.

**Assumption:** Ed25519 (Edwards-curve Digital Signature Algorithm).

**Rationale:**
- Lightweight — fits ESP32 constraints (small key size, fast verify)
- Widely adopted in embedded OTA systems (e.g. MCUboot)
- 64-byte signatures vs RSA-2048's 256 bytes — important on 512KB RAM

**Alternative considered:** RSA-2048 — more battle-tested but
significantly slower on low-power devices and uses more RAM.

**Simulation:** `ISecurity::verify_signature()` returns OK by default.
Fault injection sets `force_signature_fail = true` to simulate
invalid signature → OTA rollback path.

---

## GAP 2: WiFi Credential Storage

**What the spec says:** Provisioning flow exists but credential
storage format is unspecified.

**Assumption:** Credentials stored via `ISecurity::read_secure_secret()`
abstraction, keyed by `SecretId`. On real ESP32 this maps to NVS
(Non-Volatile Storage) namespace `device_creds`, never plain text.

**Rationale:** NVS is the standard ESP-IDF mechanism for persistent
key-value storage with optional encryption.

**Simulation:** Credentials stored as hardcoded simulation fixture.
No real NVS in host simulation — clearly marked in
`hal/sim/hal_sim.cpp`.

---

## GAP 3: Sequence Number Persistence Across Reboots

**What the spec says:** Events uploaded in sequence-number order —
no specification of how sequence numbers survive reboots.

**Assumption:** Last committed sequence number written to flash via
`IStorage::write_blob_atomic()` under key `"seq_checkpoint"`.
On recovery, `EventQueue::recover()` reads this checkpoint and
resumes from the next sequence number.

**Rationale:** Without this, sequence numbers restart at 1 after
every reboot — server cannot distinguish fresh events from
duplicates across sessions.

**Current implementation:** `event_queue.cpp` recover() restores
`next_sequence_` from scanned records.

---

## GAP 4: Boot Confirmation Timeout

**What the spec says:** "Watchdog sees all required tasks healthy
for a configurable stabilization window" — window duration
unspecified.

**Assumption:** 30-second stabilization window. All 6 tasks must
kick watchdog within this window before OTA is confirmed.

**Rationale:** Long enough for network initialization and first
sync attempt. Short enough to detect early crashes before
committing OTA slot.

**Simulation:** Watchdog timestamps tracked in `g_watchdog_timestamps`
map in `main.cpp`. Each task calls `watchdog_kick()` in its loop.

---

## GAP 5: Real HTTP Integration

**What the spec says:** `INetwork::post_json()` should communicate
with GenEd cloud endpoints.

**Assumption:** On ESP32 — `esp_http_client` from ESP-IDF.
On host simulation — architecture-correct simulation with logged
requests showing exact URL, headers, and body size.

**Gap:** Host simulation does not make real HTTP calls to
`mock_server.py`. `SimNetwork::post_json()` logs the request and
returns `HTTP 200 OK` deterministically.

**Rationale:** libcurl integration on Windows/MinGW adds significant
build complexity with no architectural benefit for evaluation.
The simulation clearly demonstrates correct endpoint targeting,
header construction, and response handling.

**Production path:** Replace `SimNetwork` with `CurlNetwork` for
host integration tests, `EspHttpNetwork` for ESP32.

---

## GAP 6: Flash Storage Format

**What the spec says:** "Avoid excessive write amplification" and
"append-only log" — no format specified.

**Assumption:** Pipe-delimited ASCII serialization for simulation.
Production would use CBOR or Protocol Buffers.

**Rationale:** ASCII is human-readable and debuggable in simulation.
CBOR is the correct production choice — compact binary, no schema
required, widely supported on embedded.

**Tradeoffs documented:**
- ASCII: readable, larger size, higher write amplification
- CBOR: compact, lower wear, requires parser
- Protobuf: schema-enforced, good tooling, heavier dependency

---

## GAP 7: Anti-Rollback Version Storage

**What the spec says:** "Anti-rollback version check" — storage
location unspecified.

**Assumption:** Minimum acceptable version stored in a protected
flash region via `IStorage::write_blob_atomic()` under key
`"min_fw_version"`. Updated only after successful OTA confirmation.
Never decremented.

**Rationale:** If stored in RAM only, a power cycle resets it —
defeating the purpose. Must survive reboot in flash.

---

## GAP 8: Rules Engine Thresholds

**What the spec says:** "More than N help requests in a rolling
time window" — N and window duration unspecified.

**Assumption:**
- `repeated_help_requests`: 3+ requests in 5 minutes
- `engagement_drop`: no activity for 10 minutes during active session
- `struggle_detected`: repeated_help_requests AND engagement_drop
  both fired within same session

**Rationale:** Conservative thresholds — tunable via config in
production. Documented in `rules_engine` module.

---

## GAP 9: Diagnostics Export Trigger

**What the spec says:** "Diagnostics export as a compact JSON
payload sent through the telemetry endpoint" — trigger unspecified.

**Assumption:** Export triggered on:
1. Explicit request via `sim_runner.py --scenario export_diagnostics`
2. Automatically after OTA completion
3. After any crash recovery

**Rationale:** Periodic export wastes bandwidth and flash writes.
Event-driven export aligns with battery-aware design.

---

## GAP 10: Compaction Policy During Low Storage

**What the spec says:** "Use explicit queue retention and
backpressure policies when storage approaches capacity" —
policy unspecified.

**Assumption:**
- Queue hard cap: 100 events (fits within RAM budget)
- Compaction triggered after every successful sync batch
- If queue reaches 80% capacity → emit `QUEUE_BACKPRESSURE` warning
- If queue reaches 100% → `enqueue()` returns `ERR_FULL`,
  new events dropped with diagnostic log

**Rationale:** Dropping unacknowledged events is never acceptable.
Dropping new events when storage is full is the only safe choice —
the alternative (overwriting old events) loses confirmed data.

---

## Summary Table

| Gap | Spec Said | Assumption Made | Confidence |
|-----|-----------|-----------------|------------|
| OTA signing | "signed" | Ed25519 | High |
| WiFi creds | "provisioning" | NVS via ISecurity | High |
| Seq persistence | "sequence order" | Flash checkpoint | High |
| Boot confirm window | "configurable" | 30 seconds | Medium |
| Real HTTP | post_json() | Simulated + logged | Medium |
| Flash format | "append-only" | ASCII sim / CBOR prod | High |
| Anti-rollback storage | "anti-rollback" | Protected flash blob | High |
| Rules thresholds | "N requests" | 3 in 5 min | Medium |
| Diagnostics trigger | "export payload" | Event-driven | Medium |
| Compaction policy | "backpressure" | 80/100 cap + drop | High |