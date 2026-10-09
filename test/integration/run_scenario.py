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
from link_topology import print_link_topology_summary  # noqa: E402
from link_events import check_link_events, default_spec_path, load_spec  # noqa: E402


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

    if scenario["mode"] in ("send", "send-recv"):
        # These modes don't proxy a persistent local socket (no TCP stub
        # client/server involved) - see run-send-recv-test.py's docstring.
        cmd = [
            sys.executable,
            str(INTEGRATION_DIR / "run-send-recv-test.py"),
            "--compose-file", str(compose_path),
            "--wait-time", str(wait_time),
            "--name", f"{args.scenario_id} Integration Test",
            "--listener-container", listener_nodes[0],
            "--connector-container", connector_nodes[0],
            "--mode", scenario["mode"],
            "--send-message", scenario.get("send_message", "Hello from integration test"),
        ]
        if scenario["mode"] == "send-recv":
            cmd += ["--reply-message", scenario.get("reply_message", "Reply from integration test")]
        return subprocess.run(cmd).returncode

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
    # Optional, opt-in scenario fields for multi-message stress tests: point
    # at alternate stub scripts and/or set how many sequential round-trip
    # messages each client exchanges (see tcp-stub-*-multimsg.py). Absent
    # from every pre-existing scenario, so default single-message behavior
    # is unaffected.
    if "stub_server" in scenario:
        cmd += ["--server-stub-path", str(INTEGRATION_DIR / scenario["stub_server"])]
    if "stub_client" in scenario:
        cmd += ["--client-stub-path", str(INTEGRATION_DIR / scenario["stub_client"])]
    if "messages" in scenario:
        cmd += ["--messages", str(scenario["messages"])]
    returncode = subprocess.run(cmd).returncode

    logs_dir = compose_path.parent / "logs"
    print_link_topology_summary(scenario, logs_dir)

    # Developer-authored link-event spec (see LINK_EVENTS_FORMAT.md), opt-in
    # per scenario - skipped entirely if scenarios/link_events/<id>.yaml doesn't exist.
    events_ok = True
    spec_path = default_spec_path(args.scenario_id)
    if spec_path.exists():
        steps = load_spec(spec_path)
        events_ok, events_lines = check_link_events(scenario, steps, logs_dir)
        print(f"\nLink-event expectations ({spec_path.relative_to(INTEGRATION_DIR)}):")
        for line in events_lines:
            print(line)
        print(f"Link-event check: {'PASSED' if events_ok else 'FAILED'}")
    else:
        print(f"\nNo link-event spec found at {spec_path.relative_to(INTEGRATION_DIR)}; skipping link-event check.")

    if not events_ok and returncode == 0:
        return 1
    return returncode


if __name__ == "__main__":
    sys.exit(main())
