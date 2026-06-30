# Host Prototype Runbook

## Build and verify

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

If CMake is unavailable, use the direct `g++` command in the README. Do not
treat a previously built executable as test proof.

## Run deterministic scenarios

```powershell
python tools/sim_runner.py --scenario all
```

A scenario succeeds only when its native test exits with code zero.

## Run the interactive host firmware

```powershell
.\build\gened_firmware.exe
```

Press Enter to request graceful task shutdown.

## Mock endpoint contracts

```powershell
python tools/mock_server.py
```

`SimNetwork` does not currently make real HTTP requests to this process. Use
the server for API-contract inspection only.

## Failure interpretation

| Output | Meaning |
|---|---|
| `storage write FAILED` | Append failed; the event must not enter RAM. |
| `commit persistence FAILED` | Acknowledgement was not saved; resend is safe. |
| `queue full` | Backpressure rejected a new event. |
| `WATCHDOG overdue` | A task exceeded its heartbeat deadline. |
| `signature INVALID` | The inactive image must not be selected. |

See `GAPS.md` before making readiness claims.

