#!/usr/bin/env python3
"""
test-timeout-zero-teardown.py

Verifies race-cli's `--timeout 0` client-connect behavior: once the local
application disconnects from race-cli's listening socket, the raceboat
conduit is torn down and the race-cli process exits immediately, instead of
waiting for a timeout window during which a reconnect would be tolerated.

Flow:
  1. Generate + bring up the racebird-client-connect-timeout-zero scenario
     (racebird channel, "timeout": 0).
  2. Exchange a few bidirectional messages over one persistent local
     connection (reusing tcp-stub-client-multimsg.py / tcp-stub-server-
     multimsg.py).
  3. Let the client stub exit normally, which closes its local TCP socket -
     this is the "client application disconnects" trigger.
  4. Poll the connector container's running state for up to
     TEARDOWN_TIMEOUT_S seconds, asserting race-cli exits (stopping the
     container) well within that window.
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

INTEGRATION_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(INTEGRATION_DIR))
from generate_scenario import generate  # noqa: E402

SCENARIO_ID = "racebird-client-connect-timeout-zero"
SERVER_CONTAINER = "listener"
CLIENT_CONTAINER = "dialer"
DEFAULT_MESSAGES = 3
TEARDOWN_TIMEOUT_S = 10


def container_running(name: str) -> bool:
    result = subprocess.run(
        ["docker", "inspect", "--format", "{{.State.Running}}", name],
        capture_output=True, text=True,
    )
    return result.returncode == 0 and result.stdout.strip() == "true"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-tag", default="working")
    parser.add_argument("--messages", type=int, default=DEFAULT_MESSAGES)
    parser.add_argument("--wait-time", type=int, default=None)
    args = parser.parse_args()

    compose_path = generate(SCENARIO_ID, args.image_tag)

    try:
        print("Starting containers...")
        subprocess.run(["docker", "compose", "-f", str(compose_path), "up", "-d"], check=True)

        print("Waiting for containers to become ready...")
        for _ in range(30):
            server_ready = subprocess.run(
                ["docker", "exec", SERVER_CONTAINER, "true"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            ).returncode == 0
            client_ready = subprocess.run(
                ["docker", "exec", CLIENT_CONTAINER, "true"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            ).returncode == 0
            if server_ready and client_ready:
                break
            time.sleep(1)
        else:
            print("FAILED: containers did not become ready in time")
            return 1

        wait_time = args.wait_time if args.wait_time is not None else 10
        print(f"Waiting {wait_time}s for RACE channel to establish...")
        time.sleep(wait_time)

        subprocess.run(
            ["docker", "cp", str(INTEGRATION_DIR / "tcp-stub-server-multimsg.py"),
             f"{SERVER_CONTAINER}:/tmp/stub-server.py"],
            check=True,
        )
        subprocess.run(
            ["docker", "cp", str(INTEGRATION_DIR / "tcp-stub-client-multimsg.py"),
             f"{CLIENT_CONTAINER}:/tmp/stub-client.py"],
            check=True,
        )

        print(f"Starting server stub (expecting {args.messages} messages)...")
        subprocess.run(
            ["docker", "exec", "-d", SERVER_CONTAINER, "bash", "-c",
             f"python3 /tmp/stub-server.py 1 {args.messages} >/tmp/stub-server.out 2>&1"],
            check=True,
        )
        time.sleep(2)

        print(f"Running client stub ({args.messages} round-trip messages, then it hangs up)...")
        client_result = subprocess.run(
            ["docker", "exec", CLIENT_CONTAINER, "python3", "/tmp/stub-client.py",
             CLIENT_CONTAINER, str(args.messages)],
            capture_output=True, text=True,
        )
        client_output = (client_result.stdout + client_result.stderr).strip()
        print(f"  client exited with code {client_result.returncode}: {client_output}")

        if not client_output.startswith("CLIENT_OK:"):
            print("FAILED: message exchange did not complete successfully before hangup")
            return 1

        # The client stub process just exited inside the container, closing
        # its local TCP connection to race-cli's --client-connect listener.
        # With --timeout 0, race-cli should tear down the conduit and exit
        # (stopping the container, since race-cli is the container's
        # foreground process) within TEARDOWN_TIMEOUT_S seconds.
        print(f"Client disconnected; polling for race-cli teardown (up to {TEARDOWN_TIMEOUT_S}s)...")
        start = time.monotonic()
        deadline = start + TEARDOWN_TIMEOUT_S
        stopped_after = None
        while time.monotonic() < deadline:
            if not container_running(CLIENT_CONTAINER):
                stopped_after = time.monotonic() - start
                break
            time.sleep(0.5)

        server_result = subprocess.run(
            ["docker", "exec", SERVER_CONTAINER, "cat", "/tmp/stub-server.out"],
            capture_output=True, text=True,
        )
        server_output = server_result.stdout.strip() if server_result.returncode == 0 else "SERVER_FAIL:no output"
        print(f"  server output: {server_output}")

        if stopped_after is None:
            print(f"FAILED: race-cli client container was still running {TEARDOWN_TIMEOUT_S}s after local disconnect")
            return 1

        print(f"PASSED: race-cli client process exited {stopped_after:.1f}s after local disconnect")
        return 0
    finally:
        print("Stopping containers...")
        subprocess.run(
            ["docker", "compose", "-f", str(compose_path), "down"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )


if __name__ == "__main__":
    sys.exit(main())
