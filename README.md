# Connectivity-Resilient ESP32 Companion Firmware Foundation

## Overview
This repository contains the simulation-first firmware foundation for the GenEd ESP32-class companion device. It is designed to survive unreliable connectivity, power loss, flash corruption, and OTA failures by implementing a robust Hardware Abstraction Layer (HAL), an event-driven architecture, and state-machine-driven modules.

## Architectural Highlights
* **Durable Event Pipeline:** Interaction events are written atomically to a durable queue (simulated flash) before network transmission.
* **Safe A/B OTA Updates:** Cryptographically verified updates with strict boot-confirmation health checks and automatic rollback capabilities.
* **Diagnostics Framework:** A flight-recorder style ring buffer that captures state transitions and crash metadata for post-mortem analysis.
* **Strict Portability:** The core business logic interacts entirely through virtual C++ interfaces (`IStorage`, `INetwork`, `IIO`).

## Folder Structure
* `/docs/`: Architecture decisions, state machines (`STATE_POWER.md`, etc.), and identified spec gaps (`GAPS.md`).
* `/firmware/hal/`: The portability boundary. Contains pure virtual interfaces and simulated implementations.
* `/firmware/event_runtime/`: Manages the durable queue and event syncing logic.
* `/firmware/ota/`: Handles safe firmware updates.
* `/firmware/connectivity/`: Manages network state and exponential backoff retry policies.
* `/tools/`: Python mock cloud endpoints and fault-injection simulation runner.

## How to Run the Simulation Setup

**1. Start the Mock Cloud Server:**
This spins up the simulated GenEd backend to receive telemetry and provide OTA payloads.
\`\`\`bash
cd tools
python3 mock_server.py
\`\`\`

**2. Compile and Run the C++ Firmware (Host Simulation):**
\`\`\`bash
mkdir build && cd build
cmake ..
make
./gened_firmware_sim
\`\`\`

**3. Inject Faults:**
Use the runner to trigger network drops or power loss during the simulation.
\`\`\`bash
python3 tools/sim_runner.py --inject-fault NETWORK_LOSS
\`\`\`

> **Note to Reviewer:** Please refer to `docs/GAPS.md` for architectural assumptions made where the specification left intentional implementation gaps.