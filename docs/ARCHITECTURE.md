# ARCHITECTURE.md — GenEd Companion Firmware

> This document describes the target architecture. It is not a completion
> claim. `GAPS.md` is the source of truth for what is implemented, tested, or
> still planned. In particular, ESP32, process-persistent flash, real HTTP and
> full OTA recovery are not complete.

## Overview

Simulation-first firmware foundation for ESP32-class GenEd companion
devices. Designed to run on host (Windows/Linux) without physical
hardware, and portable to real ESP32 by swapping HAL implementations.

---

## Core Design Principle

```
Application code NEVER touches hardware directly.
ALL hardware access goes through HAL interfaces.
```

Swap HAL implementation → same application code runs on laptop,
ESP32, or any future target.

---

## System Diagram

```
┌─────────────────────────────────────────────────┐
│              Application Layer                   │
│  TelemetryTask  SyncTask  OtaTask                │
│  PowerTask      UiTask    DiagnosticsTask        │
└────────────────────┬────────────────────────────┘
                     │ uses
┌────────────────────▼────────────────────────────┐
│           Durable Event Queue                    │
│  enqueue → storage → memory → sync → commit      │
└────────────────────┬────────────────────────────┘
                     │ uses
┌────────────────────▼────────────────────────────┐
│         HAL Interface Boundary                   │
│  IStorage  INetwork  ITime                       │
│  IPower    IIO       ISecurity                   │
└──────┬─────────────────────┬────────────────────┘
       │                     │
┌──────▼──────┐       ┌──────▼──────┐
│  SimHAL     │       │  ESP32 HAL  │
│  (laptop)   │       │  (hardware) │
└─────────────┘       └─────────────┘
```

---

## Data Flow — Happy Path

```
1. Student presses button
        ↓
2. TelemetryTask detects ButtonState::PRESSED
        ↓
3. Event created: help_requested
   {event_id, seq, timestamp, battery, rssi}
        ↓
4. EventQueue::enqueue()
   → IStorage::append_record() — flash mein pehle
   → memory queue mein add
        ↓
5. SyncTask picks up pending events
   → INetwork::post_json() → POST /telemetry
        ↓
6. Server returns HTTP 200
   → EventQueue::mark_committed()
   → IStorage::mark_record_committed()
        ↓
7. EventQueue::compact() — committed events remove
```

---

## Data Flow — Connectivity Loss

```
1. INetwork::state() → DISCONNECTED
        ↓
2. SyncTask sees offline → skips upload
   logs: "buffering (N pending)"
        ↓
3. TelemetryTask continues capturing events
   EventQueue keeps growing (max 100)
        ↓
4. PowerTask emits connectivity_lost event
        ↓
5. Network restored → INetwork::state() → CONNECTED
        ↓
6. SyncTask resumes → uploads all pending in batches
        ↓
7. Partial ack handled — only acknowledged events committed
```

---

## Data Flow — OTA Update

```
1. OtaTask polls /ota/check every 30 seconds
        ↓
2. Server returns update metadata + signature
        ↓
3. ISecurity::verify_signature() — metadata valid?
        ↓
4. Download chunks → write to inactive slot (Slot B)
        ↓
5. ISecurity::sha256() — full image hash valid?
        ↓
6. State → READY_TO_BOOT → reboot into Slot B
        ↓
7. Boot sequence runs → health checks
        ↓
8. All 6 tasks healthy for 30s → CONFIRMED
   OR crash/watchdog → ROLLBACK to Slot A
```

---

## Module Responsibilities

**`hal/`**
Single portability boundary. No application code imports
ESP-IDF or simulator APIs directly. Two implementations:
`hal/sim/` for host, `hal/esp32/` for real hardware.

**`event_runtime/`**
Durable event queue. Events assigned stable IDs before
storage write. Survives power loss via flash append log.
Recovery replays uncommitted events on boot.

**`app_runtime/`**
Boot sequencing and 6 RTOS-style tasks via `std::thread`.
On ESP32 these map directly to FreeRTOS tasks with
`xTaskCreate()`.

**`ota/`**
A/B slot state machine. Inactive slot written during
download. Active slot unchanged until boot confirmation.
Rollback if health checks fail.

**`diagnostics/`**
Ring buffer of last 100 trace events. Crash records
persist across reboots via flash. Health metrics
exported as JSON to `/diagnostics` endpoint.

**`power/`**
Battery state machine with 6 states. Low battery pauses
OTA and throttles sync. Brownout during storage/OTA
writes is fault-injected and recoverable.

**`sim/`**
Fault injection harness. Controls SimHAL state:
network disconnect, flash corruption, brownout,
signature failure, clock drift.

**`tools/`**
`mock_server.py` — Flask server with 4 core endpoints.
`sim_runner.py` — scenario runner for fault injection.

---

## RTOS Task Map

| Task | Frequency | Responsibility |
|------|-----------|----------------|
| TelemetryTask | 100ms | Button, inactivity, derived events |
| SyncTask | 500ms | Upload pending events, partial ack |
| DiagnosticsTask | 5000ms | Health metrics log |
| PowerTask | 2000ms | Battery monitor, state transitions |
| UiTask | 200ms | LED + buzzer state |
| OtaTask | 30000ms | Update check, download, verify |

---

## Inter-Task Communication

```
TelemetryTask
      │ enqueue(Event)
      ▼
EventQueue ←──── IStorage (flash)
      │
      │ get_pending()
      ▼
SyncTask ──────► INetwork (HTTP)

PowerTask ─────► g_low_battery (atomic bool)
OtaTask ───────► g_ota_in_progress (atomic bool)
ConnTask ───────► g_connected (atomic bool)

UiTask reads all 3 flags → drives LED pattern
```

No task directly mutates another task's internal state.
All shared state via atomic flags or EventQueue mutex.

---

## State Machines

Four explicit state machines — diagrams in `docs/`:

**Device Lifecycle:**
`BOOTING → PROVISIONING → ACTIVE → RECOVERY → SHUTDOWN`

**Connectivity:**
`DISCONNECTED → CONNECTING → CONNECTED → DEGRADED`

**OTA:**
`IDLE → METADATA_CHECK → DOWNLOADING → VERIFYING →
READY_TO_BOOT → PENDING_CONFIRM → CONFIRMED / ROLLBACK`

**Power:**
`ACTIVE → CONNECTED_IDLE → DISCONNECTED_BUFFERING →
LOW_BATTERY → SLEEP → RECOVERY`

---

## Portability Path to ESP32

To port from simulation to real ESP32:

1. Implement `hal/esp32/hal_esp32.cpp` with ESP-IDF APIs
2. Replace `std::thread` tasks with `xTaskCreate()`
3. Replace `std::mutex` with `xSemaphoreCreateMutex()`
4. Replace `std::queue` with `xQueueCreate()`
5. Replace file-based storage with NVS + SPIFFS
6. Application logic in `app_runtime/` — zero changes

This is the HAL boundary guarantee.

---

## Build

**Host simulation:**
```powershell
g++ -std=c++17 -I firmware/hal/include -I firmware/event_runtime
firmware/hal/sim/hal_sim.cpp
firmware/event_runtime/event_queue.cpp
firmware/app_runtime/main.cpp
-o gened_firmware.exe
```

**Mock server:**
```powershell
python tools/mock_server.py
```

**Run:**
```powershell
.\gened_firmware.exe
```
```

