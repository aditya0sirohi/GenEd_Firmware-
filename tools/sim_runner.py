#!/usr/bin/env python3
"""Run deterministic host-simulation scenarios.

The scenario checks live in tests/test_scenarios.cpp so they exercise the
same C++ HAL and event queue used by the firmware. This Python file only
selects a scenario and reports the native test program's exit code.
"""

import argparse
import subprocess
import sys
from pathlib import Path


SCENARIOS = [
    "connectivity_loss",
    "partial_ack",
    "flash_corruption",
    "low_battery",
    "bad_ota_signature",
]


def find_scenario_binary(project_root: Path) -> Path | None:
    """Find the scenario executable produced by CMake or the direct command."""
    candidates = [
        project_root / "build" / "test_scenarios.exe",
        project_root / "build" / "Debug" / "test_scenarios.exe",
        project_root / "build" / "Release" / "test_scenarios.exe",
        project_root / "tests" / "test_scenarios.exe",
        project_root / "build" / "test_scenarios",
        project_root / "tests" / "test_scenarios",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError(
        "test_scenarios executable not found. Build the host test target first."
    )


def run_scenario(binary: Path, scenario: str) -> bool:
    print(f"[SIM_RUNNER] Running deterministic scenario: {scenario}")
    result = subprocess.run([str(binary), scenario], check=False)
    return result.returncode == 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="GenEd deterministic simulation runner"
    )
    parser.add_argument("--scenario", help="Scenario name, or 'all'")
    parser.add_argument(
        "--list", action="store_true", help="List implemented scenarios"
    )
    args = parser.parse_args()

    if args.list or not args.scenario:
        print("Implemented scenarios:")
        for name in SCENARIOS:
            print(f"  {name}")
        print("  all")
        return 0

    selected = SCENARIOS if args.scenario == "all" else [args.scenario]
    unknown = [name for name in selected if name not in SCENARIOS]
    if unknown:
        print(f"Unknown or not yet implemented scenario: {unknown[0]}")
        return 2

    project_root = Path(__file__).resolve().parent.parent
    try:
        binary = find_scenario_binary(project_root)
    except FileNotFoundError as error:
        print(f"[SIM_RUNNER] ERROR: {error}")
        return 2

    failed = [
        name for name in selected if not run_scenario(binary, name)
    ]
    if failed:
        print(f"[SIM_RUNNER] FAILED: {', '.join(failed)}")
        return 1

    print(f"[SIM_RUNNER] PASS: {len(selected)} scenario(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
