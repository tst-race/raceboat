#!/usr/bin/env python3
"""
link_topology.py
Post-run diagnostic for run_scenario.py: parses race-cli debug logs to count
link create/load events per channel, and (for bootstrap-connect scenarios)
verifies the "final" slot's link topology matches the expected creator/loader
roles - the listener must CREATE exactly one private final link per
connector node (each client gets its own link, per
BootstrapPreConduitStateMachine's single-bidi-merge design), and each
connector must LOAD exactly one final link and never create one (mirroring
the hardcoded dialer-always-loads convention in BootstrapDialStateMachine).

Only the outermost per-channel wrapper (CompositeWrapper for
composition-based plugins, LoaderWrapper for plain plugins) is counted, since
a single logical createLink/loadLinkAddress call cascades through several
internal layers (ComponentManager, TransportComponentWrapper, ...) that would
otherwise be double counted.
"""

import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Tuple

INTEGRATION_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(INTEGRATION_DIR))
from generate_scenario import load_registry, load_adapter  # noqa: E402
from adapter_types import NodeRequest  # noqa: E402

LINK_EVENT_RE = re.compile(
    r"Raceboat::(?:CompositeWrapper|LoaderWrapper|PluginWrapper)::"
    r"(createLink|createLinkFromAddress|loadLinkAddress): called with"
    r".*?channelGid=([^,\s]+)"
)

CREATE_METHODS = {"createLink", "createLinkFromAddress"}

# plugin_name -> slot -> channel gid, filled in lazily by asking the plugin's
# own test adapter (avoids hardcoding plugin/channel-name mappings here).
_channel_gid_cache: Dict[Tuple[str, str], str] = {}

# channel_gid -> bool, filled in lazily from the owning plugin's manifest.json.
_merged_bidi_cache: Dict[str, bool] = {}


def is_single_bidi_channel(plugin_name: str, channel_gid: str, registry: Dict[str, Path]) -> bool:
    """Mirrors ApiContext::shouldUseSingleBidiLink() in the raceboat SDK: a
    channel only merges send+recv into one physical link (one link per
    connector) when it is both LD_BIDI and TT_UNICAST. Channels that are
    LD_BIDI but TT_MULTICAST (e.g. decomposed-exemplars' twoSixIndirect*)
    always use two separate links per connector (one send, one recv)."""
    if channel_gid in _merged_bidi_cache:
        return _merged_bidi_cache[channel_gid]
    manifest_path = registry[plugin_name] / "source" / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    props = manifest.get("channel_properties", {}).get(channel_gid, {})
    merged = (props.get("linkDirection") == "LD_BIDI" and
              props.get("transmissionType") == "TT_UNICAST")
    _merged_bidi_cache[channel_gid] = merged
    return merged


def count_link_events(log_path: Path) -> Dict[str, Dict[str, int]]:
    """Returns {channelGid: {"create": N, "load": M}} for a single node's log."""
    counts: Dict[str, Dict[str, int]] = defaultdict(lambda: {"create": 0, "load": 0})
    if not log_path.exists():
        return counts
    text = log_path.read_text(errors="replace")
    for method, channel_gid in LINK_EVENT_RE.findall(text):
        key = "create" if method in CREATE_METHODS else "load"
        counts[channel_gid][key] += 1
    return counts


def resolve_channel_gid(plugin_name: str, slot: str, registry: Dict[str, Path]) -> str:
    cache_key = (plugin_name, slot)
    if cache_key in _channel_gid_cache:
        return _channel_gid_cache[cache_key]
    adapter = load_adapter(registry[plugin_name])
    dummy_request = NodeRequest(
        role="listener", node_id="topology-probe", ip="0.0.0.0", slot=slot
    )
    contribution = adapter.generate_node_contribution(dummy_request)
    _channel_gid_cache[cache_key] = contribution.channel_name
    return contribution.channel_name


def print_link_topology_summary(scenario: dict, logs_dir: Path) -> None:
    """Informational only: total create/load counts observed for every
    channel on every node, regardless of scenario mode."""
    print("\nLink topology (all channels observed):")
    for node in scenario["nodes"]:
        log_path = logs_dir / node["id"] / f"raceboat_{node['role']}.log"
        counts = count_link_events(log_path)
        if not counts:
            print(f"  {node['id']}: (no link events found)")
            continue
        parts = ", ".join(
            f"{gid}: created={c['create']} loaded={c['load']}"
            for gid, c in sorted(counts.items())
        )
        print(f"  {node['id']}: {parts}")


def check_final_link_topology(scenario: dict, logs_dir: Path) -> Tuple[bool, List[str]]:
    """Bootstrap-only check: verifies the "final" slot's link count and
    creator/loader roles. Returns (ok, report_lines); (True, []) for
    non-bootstrap scenarios (no "final" slot to check)."""
    lines: List[str] = []
    if scenario.get("mode") != "bootstrap-connect":
        return True, lines

    nodes = scenario["nodes"]
    listeners = [n for n in nodes if n["role"] == "listener"]
    connectors = [n for n in nodes if n["role"] == "connector"]
    registry = load_registry()
    ok = True

    for listener in listeners:
        final_plugin = listener.get("slots", {}).get("final")
        if final_plugin is None:
            continue
        channel_gid = resolve_channel_gid(final_plugin, "final", registry)

        # When the same plugin (and therefore the same channelGid, e.g.
        # decomposed-exemplars' twoSixIndirectComposition used for both
        # "initial" and "final") backs both slots, the initial slot's own
        # link-reuse events (the listener creates its shared init link once
        # then loads it again for each additional accepted client; each
        # connector loads the listener's init address and creates its own
        # return link) land in the exact same log lines/channelGid bucket as
        # the final slot's - there's no reliable way to tell them apart by
        # grepping createLink/loadLinkAddress calls alone. Skip the strict
        # count assertion in that case rather than report a false mismatch.
        initial_plugin = listener.get("slots", {}).get("initial")
        if initial_plugin is not None:
            initial_channel_gid = resolve_channel_gid(initial_plugin, "initial", registry)
            if initial_channel_gid == channel_gid:
                lines.append(
                    f"  [SKIPPED] {listener['id']} (final channel={channel_gid}): "
                    f"initial slot uses the same channel gid - create/load counts "
                    f"include initial-slot link reuse and can't be reliably "
                    f"separated by log-grepping alone"
                )
                continue

        # A merged single-bidi channel (e.g. racebird's obfs4) gets one
        # physical link per connector; a non-merged channel (e.g.
        # decomposed-exemplars' twoSixIndirectComposition, TT_MULTICAST)
        # gets two - a separate send link and recv link - per connector.
        links_per_connector = 1 if is_single_bidi_channel(final_plugin, channel_gid, registry) else 2
        expected_creates = len(connectors) * links_per_connector

        log_path = logs_dir / listener["id"] / "raceboat_listener.log"
        counts = count_link_events(log_path)[channel_gid]
        node_ok = counts["create"] == expected_creates and counts["load"] == 0
        ok = ok and node_ok
        lines.append(
            f"  [{'OK' if node_ok else 'MISMATCH'}] {listener['id']} (listener, "
            f"final channel={channel_gid}): created {counts['create']} link(s) "
            f"(expected {expected_creates}, {links_per_connector} per connector), "
            f"loaded {counts['load']} (expected 0)"
        )

        for connector in connectors:
            if connector.get("slots", {}).get("final") != final_plugin:
                continue
            c_log_path = logs_dir / connector["id"] / "raceboat_connector.log"
            c_counts = count_link_events(c_log_path)[channel_gid]
            c_ok = c_counts["load"] == links_per_connector and c_counts["create"] == 0
            ok = ok and c_ok
            lines.append(
                f"  [{'OK' if c_ok else 'MISMATCH'}] {connector['id']} (connector, "
                f"final channel={channel_gid}): loaded {c_counts['load']} link(s) "
                f"(expected {links_per_connector}), created {c_counts['create']} (expected 0)"
            )

    return ok, lines
