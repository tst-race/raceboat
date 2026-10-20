#!/usr/bin/env python3
"""
run_all_scenarios.py
Runs every scenario under scenarios/*.json (each via run_scenario.py, in its
own subprocess) and prints a pass/fail summary at the end.
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

INTEGRATION_DIR = Path(__file__).resolve().parent
SCENARIOS_DIR = INTEGRATION_DIR / "scenarios"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-tag", default="main")
    parser.add_argument("--wait-time", type=int, default=None)
    parser.add_argument(
        "--only", nargs="+", default=None,
        help="Only run these scenario ids (default: all scenarios under scenarios/)",
    )
    parser.add_argument(
        "--skip", nargs="+", default=None,
        help="Scenario ids to exclude from the run",
    )
    parser.add_argument(
        "--fail-fast", action="store_true",
        help="Stop at the first failing scenario instead of running them all",
    )
    args = parser.parse_args()

    if args.only:
        scenario_ids = list(args.only)
    else:
        scenario_ids = sorted(p.stem for p in SCENARIOS_DIR.glob("*.json"))
    if args.skip:
        scenario_ids = [s for s in scenario_ids if s not in set(args.skip)]

    if not scenario_ids:
        print("No scenarios to run")
        return 1

    results: list[tuple[str, int, float]] = []
    for scenario_id in scenario_ids:
        print(f"\n{'=' * 80}\nSCENARIO: {scenario_id}\n{'=' * 80}")
        cmd = [
            sys.executable,
            str(INTEGRATION_DIR / "run_scenario.py"),
            "--scenario-id", scenario_id,
            "--image-tag", args.image_tag,
        ]
        if args.wait_time is not None:
            cmd += ["--wait-time", str(args.wait_time)]

        start = time.monotonic()
        returncode = subprocess.run(cmd).returncode
        elapsed = time.monotonic() - start
        results.append((scenario_id, returncode, elapsed))

        if returncode != 0 and args.fail_fast:
            break

    print(f"\n{'=' * 80}\nSUMMARY\n{'=' * 80}")
    failed = [r for r in results if r[1] != 0]
    for scenario_id, returncode, elapsed in results:
        status = "PASS" if returncode == 0 else f"FAIL (exit {returncode})"
        print(f"  {status:<16} {scenario_id}  ({elapsed:.1f}s)")
    print(f"\n{len(results) - len(failed)}/{len(results)} scenarios passed")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
