# MEMORY_BUDGET.md — GenEd Companion Firmware

Target: ESP32-class device
Constraints: 512 KB RAM, 8 MB Flash

---

## RAM Budget (512 KB Total)

### Task Stack Allocations

| Task | Stack Size | Rationale |
|------|-----------|-----------|
| TelemetryTask | 4 KB | Simple loop, button read, event create |
| SyncTask | 8 KB | HTTP buffers, JSON serialization |
| DiagnosticsTask | 4 KB | Metrics collection, log writes |
| PowerTask | 4 KB | Battery reads, state transitions |
| UiTask | 2 KB | LED/buzzer state, no heap alloc |
| OtaTask | 12 KB | Chunk buffers, hash computation |
| **Total Tasks** | **34 KB** | |

### Heap Allocations

| Component | RAM | Rationale |
|-----------|-----|-----------|
| EventQueue (100 events max) | 28 KB | ~280 bytes per event struct |
| OTA chunk buffer (4KB chunks) | 4 KB | One chunk at a time |
| Network TX buffer | 2 KB | JSON event payload |
| Network RX buffer | 2 KB | Server response |
| Diagnostics ring buffer (100 entries) | 8 KB | ~80 bytes per trace entry |
| Crash record (flash-backed) | 1 KB | Persisted across reboots |
| HAL sim state | 4 KB | SimStorage maps, SimNetwork state |
| **Total Heap** | **49 KB** | |

### System + FreeRTOS Overhead

| Component | RAM |
|-----------|-----|
| FreeRTOS kernel + scheduler | 10 KB |
| WiFi stack (ESP-IDF) | 60 KB |
| TCP/IP stack (lwIP) | 40 KB |
| TLS stack (mbedTLS) | 40 KB |
| Bootloader + IDF internals | 20 KB |
| **Total System** | **170 KB** |

### RAM Summary

| Category | Used |
|----------|------|
| Task stacks | 34 KB |
| Application heap | 49 KB |
| System + networking | 170 KB |
| **Total** | **253 KB** |
| **Remaining headroom** | **259 KB (50%)** |

50% headroom is intentional — ESP32 WiFi stack
usage spikes during active transmission. Headroom
prevents heap exhaustion under peak load.

---

## Flash Budget (8 MB Total)

### Partition Layout

| Partition | Size | Contents |
|-----------|------|----------|
| Bootloader | 64 KB | ESP-IDF bootloader |
| OTA Slot A (active) | 1.5 MB | Current firmware image |
| OTA Slot B (inactive) | 1.5 MB | Pending OTA firmware |
| NVS (config) | 16 KB | Device credentials, config |
| Event log | 512 KB | Durable event queue (append-only) |
| Diagnostics log | 128 KB | Crash records, trace ring buffer |
| OTA metadata | 16 KB | Slot state, version, confirmation flag |
| **Total Used** | **3.73 MB** | |
| **Remaining** | **4.27 MB** | |

### Event Queue Flash Usage

```
Max events in queue:     100
Avg event record size:   ~60 bytes (pipe-delimited)
Max queue flash usage:   6 KB active

Append-only log:         512 KB partition
Compaction frequency:    after every sync batch
Expected compaction:     every 30-60 seconds under normal use

Write amplification:     ~1.2x (append + commit marker)
```

### Flash Wear Estimate

```
Normal operation (1 session/day, 20 events/session):
  Writes/day:     ~1.2 KB
  Flash lifespan: 100,000 write cycles × 512KB
                = ~50,000 days >> product lifetime ✓

High event rate (continuous help requests):
  Writes/day:     ~50 KB
  Flash lifespan: ~1,000 days >> typical device lifetime ✓

Degraded (compaction failure, no connectivity):
  Queue fills to 100 events → backpressure
  New events dropped → ERR_FULL logged
  No unbounded growth ✓
```

### OTA Storage Budget

```
Max firmware image size:    1.5 MB (Slot A or B)
OTA chunk size:             4 KB
Chunks per update:          384 max
Interrupted download:       resumes from last chunk
Power loss during write:    chunk re-downloaded, slot
                            not marked valid until
                            full hash verified
```

---

## Queue Budget

| Queue | Max Depth | Item Size | Total |
|-------|-----------|-----------|-------|
| EventQueue (main) | 100 events | ~280 bytes | 28 KB |
| UI command queue | 10 items | 8 bytes | 80 bytes |
| OTA chunk queue | 2 chunks | 4 KB | 8 KB |

All queues are bounded. No unbounded growth anywhere
in the system. Full queues trigger backpressure,
not silent drops of unacknowledged data.

---

## Budget Validation

```
✓ Total RAM used (253 KB) < 512 KB limit
✓ 50% RAM headroom for WiFi spike protection
✓ Flash partitions fit within 8 MB
✓ Event queue bounded at 100 events
✓ OTA slots sized for realistic firmware images
✓ Flash wear within product lifetime under all scenarios
✓ No unbounded queues anywhere in design
```
```
