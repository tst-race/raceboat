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

# Valid raceboat LinkDirection values (source/common/ChannelProperties.cpp
# linkDirectionFromString) - the only manifest field
# linkDirectionOverrides is allowed to set.
LINK_DIRECTIONS = {"LD_BIDI", "LD_CREATOR_TO_LOADER", "LD_LOADER_TO_CREATOR"}

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

ADDRESS_FLAGS = {"send-address", "recv-address"}


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
        # Optional per-slot override, e.g. {"final": "twoSixIndirectCompositionReactive"}
        # - lets a scenario use a distinct channel gid for one slot so its
        # manifest entry (and any linkDirectionOverrides targeting it) stays
        # isolated from other slots sharing the same plugin.
        self.composition_names: Dict[str, str] = spec.get("composition_names", {})
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
            composition_name=node.composition_names.get(slot, node.composition_name),
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
    for slot, contribution in node.contributions.items():
        for key, value in contribution.params.items():
            parts.append(f"--param {key}={_quote_shell_arg(str(value))}")
        for flag, value in contribution.cli_flags.items():
            if slot == "final" and flag in ADDRESS_FLAGS:
                continue
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


def _apply_link_direction_overrides(scenario: dict, nodes: List[Node], output_dir: Path) -> None:
    """Test-only hook: lets a scenario "pretend" a channel's manifest-declared
    linkDirection is something other than its real value, so integration
    tests can exercise the SDK's LD_CREATOR_TO_LOADER/LD_LOADER_TO_CREATOR
    code paths (ApiContext::shouldCreateSender/shouldCreateReceiver) even
    though every plugin currently available for testing (racebird,
    decomposed-exemplars) is natively LD_BIDI. Patches only the manifest.json
    already copied into this scenario's generated/<id>/kits/<node>/ - never
    the plugin's own source/kit tree, so other scenarios/runs are unaffected.

    Keyed per (plugin, slot) - {"<plugin>": {"<slot>": "<linkDirection>"}} -
    rather than just per-plugin, since the same plugin can be used for both
    the "initial" and "final" slots (e.g. bootstrap-decomposed-decomposed)
    and only one of those slots may be a sensible one to force: in
    particular, init_send_channel (the dialer's first-contact channel to an
    out-of-band-known address) can never realistically be
    LD_CREATOR_TO_LOADER, since that would require the dialer to publish a
    fresh address for a listener that doesn't yet know this dialer exists.
    Also note plugins whose sendType is ST_EPHEM_SYNC (a live, synchronous
    socket - e.g. racebird's obfs4) can't meaningfully be forced into
    LD_CREATOR_TO_LOADER/LD_LOADER_TO_CREATOR either, regardless of slot:
    both peers still need to be simultaneously present to rendezvous, which
    is only representative of real asymmetric-role channels when paired
    with an ST_STORED_ASYNC-like (store-and-forward, e.g. whiteboard-based)
    transport such as decomposed-exemplars' twoSixIndirect.
    """
    overrides = scenario.get("linkDirectionOverrides", {})
    if not overrides:
        return
    for plugin_name, slot_overrides in overrides.items():
        for slot, link_direction in slot_overrides.items():
            if link_direction not in LINK_DIRECTIONS:
                raise ValueError(
                    f"linkDirectionOverrides: unknown linkDirection '{link_direction}' "
                    f"for plugin '{plugin_name}' slot '{slot}' "
                    f"(expected one of {sorted(LINK_DIRECTIONS)})"
                )

    patched: set = set()
    for node in nodes:
        for slot, contribution in node.contributions.items():
            plugin_name = node.slots[slot]
            slot_overrides = overrides.get(plugin_name, {})
            if slot not in slot_overrides:
                continue
            manifest_path = output_dir / "kits" / node.id / contribution.kit_dir.name / "manifest.json"
            manifest = json.loads(manifest_path.read_text())
            channel_gid = contribution.channel_name
            props = manifest.get("channel_properties", {}).get(channel_gid)
            if props is None:
                raise ValueError(
                    f"linkDirectionOverrides: channel '{channel_gid}' not found in "
                    f"channel_properties of {manifest_path} (plugin '{plugin_name}')"
                )
            props["linkDirection"] = slot_overrides[slot]
            manifest_path.write_text(json.dumps(manifest, indent=4))
            patched.add((node.id, plugin_name, slot, channel_gid, slot_overrides[slot]))
    for node_id, plugin_name, slot, channel_gid, link_direction in sorted(patched):
        print(f"  [linkDirectionOverrides] {node_id}: {plugin_name} ({channel_gid}, "
              f"slot={slot}) -> {link_direction}")


def _render_compose(scenario: dict, nodes: List[Node], output_dir: Path, image_tag: str) -> str:
    network = scenario.get("network", {"name": "race-network", "subnet": "10.18.1.0/24"})
    lines = ["services:"]
    for node in nodes:
        lines.append(f"  {node.id}:")
        lines.append(f"    image: ghcr.io/tst-race/raceboat/raceboat-runtime:{image_tag}")
        lines.append(f"    container_name: {node.id}")
        listener_ids = [n.id for n in nodes if n.role == "listener"]
        sidecars = _merge_sidecar_services(nodes)
        has_dependencies = (node.role == "connector" and listener_ids) or sidecars
        if has_dependencies:
            lines.append("    depends_on:")
        if node.role == "connector" and listener_ids:
            for listener_id in listener_ids:
                lines.append(f"      {listener_id}:")
                lines.append("        condition: service_started")
        for sidecar_name in sidecars:
            lines.append(f"      {sidecar_name}:")
            lines.append("        condition: service_healthy")
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
    _apply_link_direction_overrides(scenario, nodes, output_dir)

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
