#!/usr/bin/env python3
"""
generate_scenario.py
Central, plugin-agnostic scenario orchestrator. Reads a scenario definition
(scenarios/<id>.json) and a plugin registry (plugin_registry.json), dynamically
imports each referenced plugin's test/adapter.py, merges the resulting
per-node contributions (params, channel flags, addresses, sidecars, kits), and
emits a runnable docker-compose.yml under generated/<id>/.

No plugin-specific knowledge (param names, cert-sharing, sidecar services)
lives here - that's entirely owned by each plugin's adapter.py.
"""

import argparse
import importlib.util
import json
import shutil
import sys
from pathlib import Path
from typing import Dict, List

sys.path.insert(0, str(Path(__file__).resolve().parent))
from adapter_types import NodeContribution, NodeRequest  # noqa: E402

INTEGRATION_DIR = Path(__file__).resolve().parent
SCENARIOS_DIR = INTEGRATION_DIR / "scenarios"
GENERATED_DIR = INTEGRATION_DIR / "generated"
REGISTRY_PATH = INTEGRATION_DIR / "plugin_registry.json"

# race-cli mode flag per (scenario mode, node role).
MODE_ROLE_FLAGS = {
    "client-connect": {"listener": "server-connect", "connector": "client-connect"},
    "bootstrap-connect": {
        "listener": "server-bootstrap-connect",
        "connector": "client-bootstrap-connect",
    },
}

# --recv-channel/--send-channel flag names per scenario slot.
SLOT_CHANNEL_FLAGS = {
    "channel": ("recv-channel", "send-channel"),
    "initial": ("recv-channel", "send-channel"),
    "final": ("final-recv-channel", "final-send-channel"),
}


def load_registry() -> Dict[str, Path]:
    registry = json.loads(REGISTRY_PATH.read_text())
    return {name: (INTEGRATION_DIR / rel_path).resolve() for name, rel_path in registry.items()}


def load_adapter(plugin_dir: Path):
    adapter_path = plugin_dir / "test" / "adapter.py"
    if not adapter_path.exists():
        raise FileNotFoundError(f"No test/adapter.py found for plugin at {plugin_dir}")
    spec = importlib.util.spec_from_file_location(f"adapter_{plugin_dir.name}", adapter_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _quote_shell_arg(value: str) -> str:
    """Wrap in double quotes, escaping embedded double quotes (matches the
    hand-authored racebird compose file's convention for JSON-valued flags)."""
    return '"' + value.replace('"', '\\"') + '"'


def _yaml_flow(value) -> str:
    """Render a Python value as a YAML flow-style scalar/collection, for
    embedding adapter-supplied sidecar service fields (e.g. environment maps,
    depends_on lists) without relying on Python's repr() happening to match."""
    if isinstance(value, dict):
        return "{" + ", ".join(f"{k}: {_yaml_flow(v)}" for k, v in value.items()) + "}"
    if isinstance(value, list):
        return "[" + ", ".join(_yaml_flow(v) for v in value) + "]"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return str(value)
    return json.dumps(str(value))


class Node:
    def __init__(self, spec: dict):
        self.role = spec["role"]
        self.id = spec["id"]
        self.ip = spec["ip"]
        self.slots: Dict[str, str] = spec["slots"]  # slot -> plugin name
        self.composition_name = spec.get("composition_name")
        self.contributions: Dict[str, NodeContribution] = {}  # slot -> contribution


def _process_node(node: Node, registry: Dict[str, Path], adapters: dict, context: dict):
    for slot, plugin_name in node.slots.items():
        plugin_dir = registry[plugin_name]
        adapter = adapters.setdefault(plugin_name, load_adapter(plugin_dir))
        request = NodeRequest(
            role=node.role,
            node_id=node.id,
            ip=node.ip,
            slot=slot,
            peer_context=context.get((plugin_name, slot), {}),
            composition_name=node.composition_name,
        )
        node.contributions[slot] = adapter.generate_node_contribution(request)
        if node.role == "listener" and node.contributions[slot].address_output is not None:
            context.setdefault((plugin_name, slot), {})[node.id] = node.contributions[
                slot
            ].address_output


def _build_command(scenario: dict, node: Node) -> str:
    mode_flag = MODE_ROLE_FLAGS[scenario["mode"]][node.role]
    parts = [
        "race-cli", "-m", f"--{mode_flag}", "--debug",
        "--logto", "/tmp/raceboat_%s.log" % node.role,
        "--dir", "/kits",
    ]
    for slot, contribution in node.contributions.items():
        recv_flag, send_flag = SLOT_CHANNEL_FLAGS[slot]
        parts.append(f"--{recv_flag}={contribution.channel_name}")
        parts.append(f"--{send_flag}={contribution.channel_name}")
    if "timeout" in scenario:
        parts.append(f"--timeout={scenario['timeout']}")
    for contribution in node.contributions.values():
        for key, value in contribution.params.items():
            parts.append(f"--param {key}={_quote_shell_arg(str(value))}")
        for flag, value in contribution.cli_flags.items():
            parts.append(f"--{flag}={_quote_shell_arg(value)}")
    # YAML folded scalars (">") join lines with a single space on their own -
    # no shell-style "\" continuations, which would end up as literal
    # characters and get misparsed as escaped-space tokens by docker.
    return "\n      ".join(parts)


def _merge_sidecar_services(nodes: List[Node]) -> Dict[str, dict]:
    sidecars: Dict[str, dict] = {}
    for node in nodes:
        for contribution in node.contributions.values():
            sidecars.update(contribution.sidecar_services)
    return sidecars


def _merge_kits(node: Node, output_dir: Path) -> None:
    kits_dir = output_dir / "kits" / node.id
    kits_dir.mkdir(parents=True, exist_ok=True)
    for contribution in node.contributions.values():
        dest = kits_dir / contribution.kit_dir.name
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(contribution.kit_dir, dest)


def _render_compose(scenario: dict, nodes: List[Node], output_dir: Path, image_tag: str) -> str:
    network = scenario.get("network", {"name": "race-network", "subnet": "10.18.1.0/24"})
    lines = ["services:"]
    for node in nodes:
        lines.append(f"  {node.id}:")
        lines.append(f"    image: ghcr.io/tst-race/raceboat/raceboat-runtime:{image_tag}")
        lines.append(f"    container_name: {node.id}")
        if node.role == "connector":
            listener_ids = [n.id for n in nodes if n.role == "listener"]
            if listener_ids:
                lines.append("    depends_on:")
                for listener_id in listener_ids:
                    lines.append(f"      - {listener_id}")
        lines.append("    volumes:")
        lines.append(f"      - ./kits/{node.id}:/kits")
        lines.append(f"      - ./logs/{node.id}:/tmp")
        lines.append("    networks:")
        lines.append(f"      {network['name']}:")
        lines.append(f"        ipv4_address: {node.ip}")
        lines.append("    command: >")
        lines.append("      " + _build_command(scenario, node))
        lines.append("")

    sidecars = _merge_sidecar_services(nodes)
    # Assign static IPs to sidecars too - otherwise docker auto-assigns from the
    # start of the subnet and can collide with a node's static ipv4_address
    # depending on container start order.
    subnet_base = network["subnet"].rsplit(".", 1)[0]
    for offset, (name, definition) in enumerate(sidecars.items()):
        lines.append(f"  {name}:")
        for key, value in definition.items():
            lines.append(f"    {key}: {_yaml_flow(value)}")
        lines.append("    networks:")
        lines.append(f"      {network['name']}:")
        lines.append(f"        ipv4_address: {subnet_base}.{200 + offset}")
        lines.append("")

    lines.append("networks:")
    lines.append(f"  {network['name']}:")
    lines.append("    driver: bridge")
    lines.append("    ipam:")
    lines.append("      config:")
    lines.append(f"        - subnet: {network['subnet']}")
    lines.append("")
    return "\n".join(lines)


def generate(scenario_id: str, image_tag: str) -> Path:
    scenario = json.loads((SCENARIOS_DIR / f"{scenario_id}.json").read_text())
    registry = load_registry()
    adapters: dict = {}
    context: dict = {}

    nodes = [Node(spec) for spec in scenario["nodes"]]
    # Listeners must run first so connectors can consume their address_output.
    for node in sorted(nodes, key=lambda n: n.role != "listener"):
        _process_node(node, registry, adapters, context)

    output_dir = GENERATED_DIR / scenario_id
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)
    for node in nodes:
        _merge_kits(node, output_dir)
        (output_dir / "logs" / node.id).mkdir(parents=True, exist_ok=True)

    compose_text = _render_compose(scenario, nodes, output_dir, image_tag)
    compose_path = output_dir / "docker-compose.yml"
    compose_path.write_text(compose_text)
    return compose_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario-id", required=True)
    parser.add_argument("--image-tag", default="main", required=False)
    args = parser.parse_args()
    compose_path = generate(scenario_id=args.scenario_id, image_tag=args.image_tag)
    print(f"Generated {compose_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
