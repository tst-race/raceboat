#!/usr/bin/env python3
"""
link_topology.py
Post-run diagnostic for run_scenario.py: parses race-cli debug logs to count
link create/load events per channel, printed for informational purposes
regardless of scenario mode.

Only the outermost per-channel wrapper (CompositeWrapper for
composition-based plugins, LoaderWrapper for plain plugins) is counted, since
a single logical createLink/loadLinkAddress call cascades through several
internal layers (ComponentManager, TransportComponentWrapper, ...) that would
otherwise be double counted.
"""

import re
from collections import defaultdict
from pathlib import Path
from typing import Dict

LINK_EVENT_RE = re.compile(
    r"Raceboat::(?:CompositeWrapper|LoaderWrapper|PluginWrapper)::"
    r"(createLink|createLinkFromAddress|loadLinkAddress): called with"
    r".*?channelGid=([^,\s]+)"
)

CREATE_METHODS = {"createLink", "createLinkFromAddress"}


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
