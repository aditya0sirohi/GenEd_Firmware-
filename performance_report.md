# Firmware Budget Estimate

> These are design estimates, not measured ESP32 results. Repeatable latency,
> CPU, stack-watermark and flash-lifespan measurements remain outstanding.

## 1. System Constraints
* **Total RAM:** 512 KB 
* **Total Flash:** 8 MB
* **Hardware Target:** ESP32 (Simulation Mode)

## 2. Static RAM Allocation (Estimates)
| Component | Budget (Bytes) | Justification |
| :--- | :--- | :--- |
| **FreeRTOS Kernel & Core** | 32 KB | Base OS overhead |
| **Wi-Fi / TLS Stack** | 80 KB | Active TLS connections require heavy heap |
| **Diagnostics Ring Buffer** | 10 KB | `100 entries * 100 bytes` max |
| **Event Runtime Queue** | 20 KB | In-memory cache before flushing to flash |
| **OTA Buffer** | 16 KB | Stream chunking buffer for download |
| **Total Static/Reserved** | **~158 KB** | Safe margin for dynamic allocations |

## 3. Task Stack Budgets
All major subsystems run in isolated RTOS tasks.

| Task Name | Stack Size | Priority | Responsibility |
| :--- | :--- | :--- | :--- |
| `OtaTask` | 8 KB | High (3) | Heavy crypto (SHA256, Signature verify) |
| `SyncTask` | 6 KB | Medium (2) | TLS negotiations and HTTP parsing |
| `TelemetryTask` | 4 KB | Medium (2) | Reading from HAL, pushing to Durable Queue |
| `UiTask` | 2 KB | Low (1) | Simple GPIO LED/Buzzer toggling |
| `DiagnosticsTask`| 4 KB | Low (1) | Formatting JSON traces, checking watchdogs |
| `PowerTask` | 4 KB | Low (1) | Battery reads and state transitions |

## 4. Flash Partition Map (8 MB Total)
| Partition | Size | Purpose |
| :--- | :--- | :--- |
| `nvs` | 64 KB | Non-volatile storage (Wi-Fi creds, device ID) |
| `factory` | 2 MB | Immutable factory boot image |
| `ota_0` | 2 MB | Active application slot (A) |
| `ota_1` | 2 MB | Standby application slot (B) for safe updates |
| `spiffs_events` | 1 MB | Durable event queue storage (The "Notepad") |
| `spiffs_logs` | ~900 KB | Crash dumps and diagnostic traces |

## 5. Queue Budgets
* **Active unacknowledged queue:** 100 events. New events receive
  `ERR_FULL` at the limit; older unacknowledged events are never overwritten.
* **Simulated append log:** At most 5,000 records per logical log, with
  acknowledged records compacted when the limit is reached.
* **Derived events:** The current deterministic rules emit at most three
  derived events for one session state.
