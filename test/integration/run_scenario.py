#!/usr/bin/env python3
"""
run_scenario.py
Generates a scenario's docker-compose.yml (see generate_scenario.py) and runs
it through the existing tcp-stub-based runner (run-integration-test.py),
deriving server/client container names and additional clients from the
scenario's node roles.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

INTEGRATION_DIR = Path(__file__).resolve().parent
SCENARIOS_DIR = INTEGRATION_DIR / "scenarios"

sys.path.insert(0, str(INTEGRATION_DIR))
from generate_scenario import generate  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario-id", required=True)
    parser.add_argument("--wait-time", type=int, default=None)
    parser.add_argument("--image-tag", default="main", required=False)
    args = parser.parse_args()

    scenario = json.loads((SCENARIOS_DIR / f"{args.scenario_id}.json").read_text())
    compose_path = generate(args.scenario_id, args.image_tag)

    listener_nodes = [n["id"] for n in scenario["nodes"] if n["role"] == "listener"]
    connector_nodes = [n["id"] for n in scenario["nodes"] if n["role"] == "connector"]
    if not listener_nodes or not connector_nodes:
        print("Scenario must have at least one listener and one connector node")
        return 1

    wait_time = args.wait_time if args.wait_time is not None else scenario.get("wait_time", 10)

    cmd = [
        sys.executable,
        str(INTEGRATION_DIR / "run-integration-test.py"),
        "--compose-file", str(compose_path),
        "--wait-time", str(wait_time),
        "--name", f"{args.scenario_id} Integration Test",
        "--server-container", listener_nodes[0],
        "--client-container", connector_nodes[0],
        "--additional-clients", *connector_nodes[1:],
    ]
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
