# GenEd Companion Firmware

Host-first prototype of an ESP32 companion firmware foundation.

## Current scope

Implemented host paths include:

- HAL interfaces and deterministic simulation implementations;
- bounded event queue with acknowledgement and logical-reboot recovery;
- six thread-based task loops;
- deterministic rules and power-state behavior;
- diagnostics trace/crash primitives;
- a basic host A/B OTA state model;
- executable unit tests and five deterministic scenario checks.

The ESP32 HAL, real host HTTP transport, full interruption-safe flash model and
production OTA flow remain incomplete. See `docs/GAPS.md`.

## Requirements

- C++17 compiler with thread support
- CMake 3.16 or newer (recommended)
- Python 3 for the scenario wrapper and mock endpoint server
- Flask only when running `tools/mock_server.py`

Python test dependency:

```powershell
python -m pip install -r requirements-dev.txt
```

## Build and test with CMake

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The current machine must have CMake installed. A direct compiler command can
be used when CMake is unavailable:

```powershell
g++ -std=c++17 -pthread -I firmware -I firmware/hal/include -I firmware/event_runtime firmware/hal/sim/hal_sim.cpp firmware/event_runtime/event_queue.cpp firmware/connectivity/connectivity.cpp firmware/diagnostics/diagnostics.cpp firmware/ota/ota.cpp firmware/power/power_manager.cpp firmware/rules_engine/rules_engine.cpp firmware/app_runtime/main.cpp -o gened_firmware.exe
```

## Run deterministic scenarios

After building `test_scenarios`:

```powershell
python tools/sim_runner.py --list
python tools/sim_runner.py --scenario connectivity_loss
python tools/sim_runner.py --scenario all
```

Implemented scenarios:

- connectivity loss, buffering and recovery;
- partial acknowledgement;
- flash append failure and subsequent recovery;
- low-battery transition and recovery;
- invalid OTA signature rejection.

The runner returns a non-zero exit code when an assertion fails.

## Mock endpoint contracts

```powershell
python tools/mock_server.py
```

The Flask server exposes provisioning, telemetry, batch acknowledgement, OTA
and diagnostics contracts. `SimNetwork` currently models responses in-process;
it does not connect to this server over a real socket.

## Repository structure

```text
firmware/hal/           HAL interfaces, host simulation and ESP32 declarations
firmware/app_runtime/   Boot path and six host task loops
firmware/event_runtime/ Event model and bounded recovery queue
firmware/connectivity/  Retry-oriented connectivity wrapper
firmware/diagnostics/   Trace, error and crash primitives
firmware/ota/           Basic host A/B update model
firmware/power/         Power-state selection
firmware/rules_engine/  Deterministic derived-event rules
tests/                  Executable unit and scenario checks
tools/                  Scenario wrapper and mock endpoint server
docs/                   Architecture, state diagrams, budgets and gaps
```
