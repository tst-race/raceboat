#!/usr/bin/env python3
"""
generate_scenario.py
Central, plugin-agnostic scenario orchestrator. Reads a scenario definition
(scenarios/<id>.json) naming the CHANNEL/COMPOSITION gid each node's slot
uses (e.g. "obfs4", "twoSixIndirectComposition", "skyhookBasicComposition"),
scans every plugin in plugin_registry.json's built manifest.json to figure
out which plugin(s) actually implement that gid, dynamically imports each
needed plugin's test/adapter.py, merges the resulting per-node contributions
(params, channel flags, addresses, sidecars, kits), and emits a runnable
docker-compose.yml under generated/<id>/.

Plugin discovery is fully automatic: a channel gid can be a plain "unified"
plugin channel (manifest.json plugins[].channels, e.g. racebird's "obfs4") or
a decomposed "composition" (manifest.json compositions[], e.g.
decomposed-exemplars' "twoSixIndirectComposition") whose transport/usermodel/
encoding components may each be implemented by a DIFFERENT plugin - see
raceboat/source/plugin-loading/PluginLoader.cpp: every manifest.json found
under --dir is parsed and ALL plugins' transports/usermodels/encodings are
merged into one global namespace before a composition is resolved, so e.g.
skyhook's compositions reference an encoding ("noop") it doesn't itself
implement - decomposed-exemplars provides it, and build_channel_registry()
below figures that out automatically as long as both plugins are listed in
plugin_registry.json. No plugin-specific knowledge (param names, cert-sharing,
sidecar services) lives here - that's entirely owned by each plugin's
adapter.py.
"""

import argparse
import importlib.util
import json
import os
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Tuple, Union

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


def _host_user_directive() -> str:
    """Node containers default to root (no USER in raceboat-runtime-image), so
    anything they write into bind-mounted ./kits/./logs under generated/<id>/
    ends up root-owned on the host. Running as the invoking host user/group
    instead makes those files owned by the user, matching the rest of
    generated/ and avoiding a `sudo rm -rf` to clean up."""
    return f"{os.getuid()}:{os.getgid()}"


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


# A scenario slot's channel/composition gid can be a plain string (one gid
# fills both the recv and send direction, the common case) or a dict
# {"recv": gid, "send": gid} when the two directions of one logical phase
# (e.g. bootstrap's initial upstream vs downstream, or a plain client-connect
# test's upstream vs downstream) should use genuinely different channels -
# whether that's two compositions of the same plugin (e.g.
# twoSixIndirectComposition vs twoSixIndirectCompositionReactive) or two
# entirely different plugins (e.g. obfs4 vs twoSixIndirectComposition).
SlotSpec = Union[str, Dict[str, str]]


@dataclass
class ChannelInfo:
    channel_name: str
    # False for a plain "unified" plugin channel (manifest plugins[].channels);
    # True for a decomposed "composition" (manifest compositions[]).
    is_composition: bool
    # Ordered, deduplicated (plugin_name, kit_dir) pairs needed to load this
    # channel. plugin_dirs[0] (the "owner") is the plugin whose manifest
    # actually declares this channel_name (its channel_properties/composition
    # entry) and whose adapter drives this node's params/addressing/sidecars;
    # any remaining entries are "donor" plugins that implement a component
    # (e.g. an encoding) the owner's composition references but doesn't
    # itself implement - see build_channel_registry().
    plugin_dirs: List[Tuple[str, Path]] = field(default_factory=list)


def build_channel_registry(
    registry: Dict[str, Path], adapters: dict, role: str,
) -> Dict[str, ChannelInfo]:
    """Scans every plugin in plugin_registry.json's manifest.json (as built
    for the given node role - see adapter.py's kit_dir(role)) to build a map
    from channel/composition gid -> the plugin(s) that must be present under
    /kits to load it. This is what lets a scenario reference a channel or
    composition gid directly instead of naming a plugin_registry.json entry:
    plugin discovery is fully automatic, including cross-plugin compositions
    (see module docstring)."""
    kit_dir_by_plugin: Dict[str, Path] = {}
    component_owner: Dict[str, Tuple[str, Path]] = {}
    composition_components: Dict[str, List[str]] = {}
    composition_declared_by: Dict[str, str] = {}
    channel_registry: Dict[str, ChannelInfo] = {}

    for plugin_name, plugin_dir in registry.items():
        adapter = adapters.setdefault(plugin_name, load_adapter(plugin_dir))
        kit_dir = adapter.kit_dir(role)
        manifest_path = kit_dir / "manifest.json"
        if not manifest_path.exists():
            # Not every plugin in plugin_registry.json needs to be built for
            # every scenario - skip it here and let resolution below fail
            # with a clear "missing component"/"unknown channel" error only
            # if a scenario actually needs something this plugin provides.
            print(f"  [channel registry] skipping '{plugin_name}' (role={role}): "
                  f"no manifest.json at {manifest_path} - not built?")
            continue
        kit_dir_by_plugin[plugin_name] = kit_dir
        manifest = json.loads(manifest_path.read_text())
        for plugin_def in manifest.get("plugins", []):
            for channel_gid in plugin_def.get("channels", []):
                channel_registry[channel_gid] = ChannelInfo(
                    channel_gid, False, [(plugin_name, kit_dir)]
                )
            for component_key in ("transports", "usermodels", "encodings"):
                for component_name in plugin_def.get(component_key, []):
                    component_owner[component_name] = (plugin_name, kit_dir)
        for composition in manifest.get("compositions", []):
            composition_id = composition["id"]
            composition_components[composition_id] = (
                [composition["transport"], composition["usermodel"]]
                + list(composition.get("encodings", []))
            )
            composition_declared_by[composition_id] = plugin_name

    for composition_name, component_names in composition_components.items():
        owner_plugin = composition_declared_by[composition_name]
        plugin_dirs: List[Tuple[str, Path]] = [(owner_plugin, kit_dir_by_plugin[owner_plugin])]
        seen_plugins = {owner_plugin}
        missing: List[str] = []
        for component_name in component_names:
            owner = component_owner.get(component_name)
            if owner is None:
                missing.append(component_name)
                continue
            if owner[0] not in seen_plugins:
                seen_plugins.add(owner[0])
                plugin_dirs.append(owner)
        if missing:
            raise ValueError(
                f"composition '{composition_name}' (role={role}) needs component(s) "
                f"{missing} but no plugin registered in plugin_registry.json provides "
                f"them (scanned: {sorted(registry)}). Add the plugin that implements "
                f"{missing} to plugin_registry.json."
            )
        channel_registry[composition_name] = ChannelInfo(composition_name, True, plugin_dirs)

    return channel_registry


class Node:
    def __init__(self, spec: dict):
        self.role = spec["role"]
        self.id = spec["id"]
        self.ip = spec["ip"]
        self.slots: Dict[str, SlotSpec] = spec["slots"]  # slot -> channel/composition gid(s)
        # (slot, "recv"|"send") -> contribution. Directions only share the SAME
        # contribution instance (one adapter call) when both directions name
        # the same channel/composition gid (the common case).
        self.contributions: Dict[Tuple[str, str], NodeContribution] = {}
        self.contribution_channels: Dict[Tuple[str, str], str] = {}
        # Accumulated across every slot/direction this node uses, keyed by
        # kit dir name to dedup - the full set of plugin kits that must be
        # merged into this node's /kits for every channel it was given to load.
        self.needed_kit_dirs: Dict[str, Tuple[str, Path]] = {}


def _process_node(
    node: Node, channel_registry: Dict[str, ChannelInfo], adapters: dict,
    context: dict, resolution_log: List[str],
):
    for slot, channel_spec in node.slots.items():
        directions = (
            channel_spec if isinstance(channel_spec, dict)
            else {"recv": channel_spec, "send": channel_spec}
        )
        # "split" when recv and send name different channel/composition gids -
        # vs. the common case of one shared bidi contribution filling both
        # directions. See NodeRequest.direction for why adapters need to know.
        split = directions["recv"] != directions["send"]

        contribution_cache: Dict[str, NodeContribution] = {}
        for direction, channel_name in directions.items():
            contribution = contribution_cache.get(channel_name)
            if contribution is None:
                info = channel_registry.get(channel_name)
                if info is None:
                    raise ValueError(
                        f"node '{node.id}' (role={node.role}) slot '{slot}': unknown "
                        f"channel/composition '{channel_name}'. Known channels/compositions "
                        f"for this role: {sorted(channel_registry)}"
                    )
                owner_plugin, owner_kit_dir = info.plugin_dirs[0]
                adapter = adapters[owner_plugin]
                peer_context = context.get((channel_name, slot), {})
                request = NodeRequest(
                    role=node.role,
                    node_id=node.id,
                    ip=node.ip,
                    slot=slot,
                    peer_context=peer_context,
                    composition_name=channel_name if info.is_composition else None,
                    direction=direction if split else None,
                )
                contribution = adapter.generate_node_contribution(request)
                contribution.channel_name = channel_name
                for plugin_name, kit_dir in info.plugin_dirs:
                    node.needed_kit_dirs[kit_dir.name] = (plugin_name, kit_dir)
                contribution_cache[channel_name] = contribution
                if node.role == "listener" and contribution.address_output is not None:
                    context.setdefault((channel_name, slot), {})[node.id] = (
                        contribution.address_output
                    )
                resolution_log.append(
                    f"  [channel] {node.id} ({node.role}) slot={slot}: '{channel_name}' "
                    + ("(composition)" if info.is_composition else "(channel)")
                    + " -> plugins " + str([p for p, _ in info.plugin_dirs])
                )
            node.contributions[(slot, direction)] = contribution
            node.contribution_channels[(slot, direction)] = channel_name


def _build_command(scenario: dict, node: Node) -> str:
    mode_flag = MODE_ROLE_FLAGS[scenario["mode"]][node.role]
    parts = [
        "race-cli", "-m", f"--{mode_flag}", "--debug",
        "--logto", "/tmp/raceboat_%s.log" % node.role,
        "--dir", "/kits",
    ]
    for slot in node.slots:
        recv_flag, send_flag = SLOT_CHANNEL_FLAGS[slot]
        parts.append(f"--{recv_flag}={node.contributions[(slot, 'recv')].channel_name}")
        parts.append(f"--{send_flag}={node.contributions[(slot, 'send')].channel_name}")
    if "timeout" in scenario:
        parts.append(f"--timeout={scenario['timeout']}")
    seen_contributions: set = set()
    for (slot, direction), contribution in node.contributions.items():
        if id(contribution) in seen_contributions:
            continue
        seen_contributions.add(id(contribution))
        # True whenever this slot's recv and send directions were resolved to
        # genuinely different contributions - either a mixed {"recv": gidA,
        # "send": gidB} slot naming two different plugins, or two different
        # compositions of the SAME plugin - vs. the common case of one
        # channel/composition gid filling both directions via a single
        # shared call.
        distinct_directions = (
            node.contributions[(slot, "recv")] is not node.contributions[(slot, "send")]
        )
        for key, value in contribution.params.items():
            parts.append(f"--param {key}={_quote_shell_arg(str(value))}")
        for flag, value in contribution.cli_flags.items():
            if slot == "final" and flag in ADDRESS_FLAGS:
                continue
            if distinct_directions and flag in ADDRESS_FLAGS:
                # This contribution fills only ONE direction of the slot - its
                # adapter names the address flag from the NODE's role
                # (listener always calls it "recv-address", connector
                # "send-address") regardless of which direction it actually
                # serves here, so attribute it to the flag matching the
                # direction it's actually assigned to in this slot.
                flag = "recv-address" if direction == "recv" else "send-address"
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
    for _plugin_name, kit_dir in node.needed_kit_dirs.values():
        dest = kits_dir / kit_dir.name
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(kit_dir, dest)


def _apply_link_direction_overrides(scenario: dict, nodes: List[Node], output_dir: Path) -> None:
    """Test-only hook: lets a scenario "pretend" a channel's manifest-declared
    linkDirection is something other than its real value, so integration
    tests can exercise the SDK's LD_CREATOR_TO_LOADER/LD_LOADER_TO_CREATOR
    code paths (ApiContext::shouldCreateSender/shouldCreateReceiver) even
    though every plugin currently available for testing (racebird,
    decomposed-exemplars) is natively LD_BIDI. Patches only the manifest.json
    already copied into this scenario's generated/<id>/kits/<node>/ - never
    the plugin's own source/kit tree, so other scenarios/runs are unaffected.

    Keyed per (channel/composition gid, slot) -
    {"<channel_gid>": {"<slot>": "<linkDirection>"}} - rather than just per
    channel, since the same channel/composition can be used for both the
    "initial" and "final" slots (e.g. bootstrap-decomposed-decomposed) and
    only one of those slots may be a sensible one to force: in particular,
    init_send_channel (the dialer's first-contact channel to an
    out-of-band-known address) can never realistically be
    LD_CREATOR_TO_LOADER, since that would require the dialer to publish a
    fresh address for a listener that doesn't yet know this dialer exists.
    Also note channels whose sendType is ST_EPHEM_SYNC (a live, synchronous
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
    for channel_name, slot_overrides in overrides.items():
        for slot, link_direction in slot_overrides.items():
            if link_direction not in LINK_DIRECTIONS:
                raise ValueError(
                    f"linkDirectionOverrides: unknown linkDirection '{link_direction}' "
                    f"for channel '{channel_name}' slot '{slot}' "
                    f"(expected one of {sorted(LINK_DIRECTIONS)})"
                )

    patched: set = set()
    for node in nodes:
        seen_contributions: set = set()
        for (slot, _direction), contribution in node.contributions.items():
            if id(contribution) in seen_contributions:
                continue
            seen_contributions.add(id(contribution))
            channel_name = node.contribution_channels[(slot, _direction)]
            slot_overrides = overrides.get(channel_name, {})
            if slot not in slot_overrides:
                continue
            manifest_path = output_dir / "kits" / node.id / contribution.kit_dir.name / "manifest.json"
            manifest = json.loads(manifest_path.read_text())
            props = manifest.get("channel_properties", {}).get(channel_name)
            if props is None:
                raise ValueError(
                    f"linkDirectionOverrides: channel '{channel_name}' not found in "
                    f"channel_properties of {manifest_path}"
                )
            props["linkDirection"] = slot_overrides[slot]
            manifest_path.write_text(json.dumps(manifest, indent=4))
            patched.add((node.id, slot, channel_name, slot_overrides[slot]))
    for node_id, slot, channel_name, link_direction in sorted(patched):
        print(f"  [linkDirectionOverrides] {node_id}: {channel_name} "
              f"(slot={slot}) -> {link_direction}")


def _render_compose(scenario: dict, nodes: List[Node], output_dir: Path, image_tag: str) -> str:
    network = scenario.get("network", {"name": "race-network", "subnet": "10.18.1.0/24"})
    lines = ["services:"]
    for node in nodes:
        lines.append(f"  {node.id}:")
        lines.append(f"    image: ghcr.io/tst-race/raceboat/raceboat-runtime:{image_tag}")
        lines.append(f"    container_name: {node.id}")
        lines.append(f'    user: "{_host_user_directive()}"')
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
    channel_registries: Dict[str, Dict[str, ChannelInfo]] = {}
    resolution_log: List[str] = []

    nodes = [Node(spec) for spec in scenario["nodes"]]
    # Listeners must run first so connectors can consume their address_output.
    for node in sorted(nodes, key=lambda n: n.role != "listener"):
        if node.role not in channel_registries:
            channel_registries[node.role] = build_channel_registry(registry, adapters, node.role)
        _process_node(node, channel_registries[node.role], adapters, context, resolution_log)

    # Explicit feedback on which plugin(s) were pulled in for each
    # channel/composition gid used by this scenario, so a developer reading a
    # scenario file doesn't have to go spelunking through manifests to know
    # which plugin(s) a given channel name resolves to.
    print("Resolved channels/compositions:")
    for line in resolution_log:
        print(line)

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


def list_channels(role: str) -> None:
    """Prints every channel/composition gid resolvable for the given role and
    which plugin(s) it needs - self-documentation for a developer writing or
    reviewing a scenario who doesn't know which plugin owns a given gid."""
    registry = load_registry()
    adapters: dict = {}
    channel_registry = build_channel_registry(registry, adapters, role)
    for channel_name in sorted(channel_registry):
        info = channel_registry[channel_name]
        kind = "composition" if info.is_composition else "channel"
        plugins = [p for p, _ in info.plugin_dirs]
        print(f"  {channel_name} ({kind}) -> plugins {plugins}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario-id", required=False)
    parser.add_argument("--image-tag", default="main", required=False)
    parser.add_argument(
        "--list-channels", metavar="ROLE", choices=["listener", "connector"],
        help="Print every channel/composition gid available for ROLE and which "
             "plugin(s) it resolves to, then exit (no scenario needed).",
    )
    args = parser.parse_args()
    if args.list_channels:
        list_channels(args.list_channels)
        return 0
    if not args.scenario_id:
        parser.error("--scenario-id is required unless --list-channels is given")
    compose_path = generate(scenario_id=args.scenario_id, image_tag=args.image_tag)
    print(f"Generated {compose_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
