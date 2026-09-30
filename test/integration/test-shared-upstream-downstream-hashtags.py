#!/usr/bin/env python3
"""
test-shared-upstream-downstream-hashtags.py

Verifies twoSixIndirect can multiplex many clients over exactly TWO
pre-specified, fixed link addresses instead of one link per client:
  - an "upstream" hashtag every client POSTs to and the server FETCHES from,
    so the server needs only ONE fetch/receive action in its user model to
    pick up every client's message, regardless of how many clients there are
  - a "downstream" hashtag the server POSTs to and every client FETCHES
    from, so the server can reply to many clients by stuffing all their
    replies into a single post to one shared hashtag, instead of one post
    per client

This bypasses generate_scenario.py's listener/connector peer-address-sharing
model (a connector only ever gets ONE address there, for --send-address,
discovered via the listener's address_output) since here BOTH addresses are
picked up front as fixed constants and given identically to every node - so
this script builds its own docker-compose.yml directly. It still reuses
decomposed-exemplars' test/adapter.py for the kit directory and sidecar
service definitions, so it doesn't hand-duplicate that plugin-owned info.

Exercises race-cli's --recv-address support for --client-connect (a client
loading a pre-specified reply address instead of creating its own one-off
address), added specifically to make this scenario possible.
"""

import json
import shutil
import subprocess
import sys
import uuid
from pathlib import Path

INTEGRATION_DIR = Path(__file__).resolve().parent
DECOMPOSED_TEST_DIR = INTEGRATION_DIR.parents[2] / "decomposed-exemplars" / "test"
sys.path.insert(0, str(INTEGRATION_DIR))
sys.path.insert(0, str(DECOMPOSED_TEST_DIR))
from adapter_types import NodeRequest  # noqa: E402
from generate_scenario import _quote_shell_arg, _yaml_flow  # noqa: E402
import adapter as decomposed_adapter  # noqa: E402

GENERATED_DIR = INTEGRATION_DIR / "generated" / "shared-upstream-downstream-hashtags"
COMPOSE_PATH = GENERATED_DIR / "docker-compose.yml"
# Preserved outside GENERATED_DIR so a subsequent run's generate() (which wipes
# GENERATED_DIR) doesn't destroy the previous run's logs before they can be
# manually examined.
PRESERVED_LOGS_DIR = INTEGRATION_DIR / "generated" / "shared-upstream-downstream-hashtags-last-run-logs"
IMAGE_TAG_DEFAULT = "main"

NETWORK_NAME = "shared-ud-network"
SUBNET = "10.32.1.0/24"
SERVER_ID = "listener"
CLIENT_IDS = ["dialer", "dialer2"]
SERVER_IP = "10.32.1.3"
CLIENT_IPS = {"dialer": "10.32.1.2", "dialer2": "10.32.1.4"}

DEFAULT_MESSAGES = 10
TEARDOWN_TIMEOUT_S = 10


def _make_address() -> dict:
    return {
        "hashtag": f"shared_{uuid.uuid4().hex[:12]}",
        "hostname": decomposed_adapter.WHITEBOARD_HOSTNAME,
        "port": decomposed_adapter.WHITEBOARD_PORT,
    }


def _build_command(role: str, node_id: str, upstream: dict, downstream: dict) -> str:
    mode_flag = "server-connect" if role == "listener" else "client-connect"
    parts = [
        "race-cli", "-m", f"--{mode_flag}", "--debug",
        "--logto", "/tmp/raceboat_%s.log" % ("listener" if role == "listener" else "connector"),
        "--dir", "/kits",
        f"--recv-channel={decomposed_adapter.DEFAULT_COMPOSITION}",
        f"--send-channel={decomposed_adapter.DEFAULT_COMPOSITION}",
    ]
    if role == "listener":
        # Server fetches every client's message from the one shared upstream
        # hashtag, and now reuses ONE preset send link (ReceiveOptions::
        # send_address) to post replies to the shared downstream hashtag for
        # every accepted conduit, instead of creating a fresh duplicate link
        # per client from the address each dial message happens to carry.
        parts.append(f"--recv-address={_quote_shell_arg(json.dumps(upstream))}")
        parts.append(f"--send-address={_quote_shell_arg(json.dumps(downstream))}")
    else:
        # Client posts to the shared upstream hashtag (like any connector
        # dialing a published listener address) and LOADS the pre-specified
        # shared downstream hashtag for its own reply address, instead of
        # creating a fresh one-off address per client.
        parts.append(f"--send-address={_quote_shell_arg(json.dumps(upstream))}")
        parts.append(f"--recv-address={_quote_shell_arg(json.dumps(downstream))}")
    return "\n      ".join(parts)


def generate(image_tag: str) -> Path:
    if GENERATED_DIR.exists():
        shutil.rmtree(GENERATED_DIR)
    GENERATED_DIR.mkdir(parents=True)

    upstream = _make_address()
    downstream = _make_address()
    print(f"Upstream (client-send/server-recv) address: {json.dumps(upstream)}")
    print(f"Downstream (server-send/client-recv) address: {json.dumps(downstream)}")

    kit_dir = decomposed_adapter.generate_node_contribution(
        NodeRequest(role="listener", node_id=SERVER_ID, ip=SERVER_IP, slot="channel")
    ).kit_dir
    sidecars = dict(decomposed_adapter.SIDECAR_SERVICES)

    all_node_ids = [SERVER_ID] + CLIENT_IDS
    for node_id in all_node_ids:
        dest = GENERATED_DIR / "kits" / node_id / kit_dir.name
        shutil.copytree(kit_dir, dest)
        (GENERATED_DIR / "logs" / node_id).mkdir(parents=True, exist_ok=True)

    lines = ["services:"]
    for node_id, role, ip in [(SERVER_ID, "listener", SERVER_IP)] + [
        (client_id, "connector", CLIENT_IPS[client_id]) for client_id in CLIENT_IDS
    ]:
        lines.append(f"  {node_id}:")
        lines.append(f"    image: ghcr.io/tst-race/raceboat/raceboat-runtime:{image_tag}")
        lines.append(f"    container_name: {node_id}")
        lines.append("    depends_on:")
        if role == "connector":
            lines.append(f"      {SERVER_ID}:")
            lines.append("        condition: service_started")
        for sidecar_name in sidecars:
            lines.append(f"      {sidecar_name}:")
            lines.append("        condition: service_healthy")
        lines.append("    volumes:")
        lines.append(f"      - ./kits/{node_id}:/kits")
        lines.append(f"      - ./logs/{node_id}:/tmp")
        lines.append("    networks:")
        lines.append(f"      {NETWORK_NAME}:")
        lines.append(f"        ipv4_address: {ip}")
        lines.append("    command: >")
        lines.append("      " + _build_command(role, node_id, upstream, downstream))
        lines.append("")

    for offset, (sidecar_name, sidecar) in enumerate(sidecars.items()):
        lines.append(f"  {sidecar_name}:")
        for key, value in sidecar.items():
            lines.append(f"    {key}: {_yaml_flow(value)}")
        lines.append("    networks:")
        lines.append(f"      {NETWORK_NAME}:")
        lines.append(f"        ipv4_address: 10.32.1.{200 + offset}")
        lines.append("")

    lines.append("networks:")
    lines.append(f"  {NETWORK_NAME}:")
    lines.append("    driver: bridge")
    lines.append("    ipam:")
    lines.append("      config:")
    lines.append(f"        - subnet: {SUBNET}")

    COMPOSE_PATH.write_text("\n".join(lines) + "\n")
    return COMPOSE_PATH


def container_running(name: str) -> bool:
    result = subprocess.run(
        ["docker", "inspect", "--format", "{{.State.Running}}", name],
        capture_output=True, text=True,
    )
    return result.returncode == 0 and result.stdout.strip() == "true"


def main() -> int:
    import argparse
    import time

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-tag", default=IMAGE_TAG_DEFAULT)
    parser.add_argument("--messages", type=int, default=DEFAULT_MESSAGES)
    parser.add_argument("--wait-time", type=int, default=15)
    args = parser.parse_args()

    compose_path = generate(args.image_tag)

    try:
        print("Starting containers...")
        subprocess.run(["docker", "compose", "-f", str(compose_path), "up", "-d"], check=True)

        print("Waiting for containers to become ready...")
        all_nodes = [SERVER_ID] + CLIENT_IDS
        for _ in range(30):
            if all(
                subprocess.run(
                    ["docker", "exec", node, "true"],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                ).returncode == 0
                for node in all_nodes
            ):
                break
            time.sleep(1)
        else:
            print("FAILED: containers did not become ready in time")
            return 1

        print(f"Waiting {args.wait_time}s for RACE channel to establish...")
        time.sleep(args.wait_time)

        subprocess.run(
            ["docker", "cp", str(INTEGRATION_DIR / "tcp-stub-server-multimsg.py"),
             f"{SERVER_ID}:/tmp/stub-server.py"],
            check=True,
        )
        for client_id in CLIENT_IDS:
            subprocess.run(
                ["docker", "cp", str(INTEGRATION_DIR / "tcp-stub-client-multimsg.py"),
                 f"{client_id}:/tmp/stub-client.py"],
                check=True,
            )

        num_clients = len(CLIENT_IDS)
        print(f"Starting server stub (expecting {num_clients} clients, {args.messages} messages each)...")
        subprocess.run(
            ["docker", "exec", "-d", SERVER_ID, "bash", "-c",
             f"python3 /tmp/stub-server.py {num_clients} {args.messages} >/tmp/stub-server.out 2>&1"],
            check=True,
        )
        time.sleep(2)

        print(f"Starting {num_clients} client stub(s)...")
        import threading
        client_results = [None] * num_clients

        def run_client_stub(idx, client_id):
            client_results[idx] = subprocess.run(
                ["docker", "exec", client_id, "python3", "/tmp/stub-client.py",
                 client_id, str(args.messages)],
                capture_output=True, text=True,
            )

        threads = [
            threading.Thread(target=run_client_stub, args=(idx, client_id))
            for idx, client_id in enumerate(CLIENT_IDS)
        ]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        all_ok = True
        for client_id, result in zip(CLIENT_IDS, client_results):
            output = (result.stdout + result.stderr).strip()
            print(f"  {client_id} exited with code {result.returncode}: {output}")
            if not output.startswith("CLIENT_OK:"):
                all_ok = False

        # The server stub has its own, independent per-message timeout, so it
        # can still be waiting on a client that already gave up - poll until
        # it actually finishes (or give up after its own timeout budget).
        print("Waiting for server stub to finish...")
        server_stub_timeout_s = 320
        for _ in range(server_stub_timeout_s):
            still_running = subprocess.run(
                ["docker", "exec", SERVER_ID, "pgrep", "-f", "stub-server.py"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            ).returncode == 0
            if not still_running:
                break
            time.sleep(1)

        server_result = subprocess.run(
            ["docker", "exec", SERVER_ID, "cat", "/tmp/stub-server.out"],
            capture_output=True, text=True,
        )
        server_output = server_result.stdout.strip() if server_result.returncode == 0 else "SERVER_FAIL:no output"
        print(f"  server output: {server_output}")
        if not server_output.startswith("SERVER_OK:"):
            all_ok = False

        if not all_ok:
            print("FAILED: not all clients/server completed successfully")
            return 1

        print(f"PASSED: {num_clients} clients exchanged {args.messages} messages each "
              f"over one shared upstream + one shared downstream hashtag")
        return 0
    finally:
        if PRESERVED_LOGS_DIR.exists():
            shutil.rmtree(PRESERVED_LOGS_DIR)
        if (GENERATED_DIR / "logs").exists():
            shutil.copytree(GENERATED_DIR / "logs", PRESERVED_LOGS_DIR)
            print(f"Preserved this run's logs at {PRESERVED_LOGS_DIR}")
        print("Stopping containers...")
        subprocess.run(
            ["docker", "compose", "-f", str(compose_path), "down"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )


if __name__ == "__main__":
    sys.exit(main())
