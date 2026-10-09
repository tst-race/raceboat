#!/usr/bin/env python3
"""
link_events.py
Developer-facing replacement for link_topology.py's hardcoded final-link
assumptions: checks a scenario's debug logs against a declarative
scenarios/link_events/<scenario-id>.yaml spec describing the expected
sequence of link CREATE/LOAD operations. See LINK_EVENTS_FORMAT.md for the
full format description and semantics.
"""

import argparse
import json
import re
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import yaml

INTEGRATION_DIR = Path(__file__).resolve().parent
SCENARIOS_DIR = INTEGRATION_DIR / "scenarios"
LINK_EVENTS_DIR = SCENARIOS_DIR / "link_events"
GENERATED_DIR = INTEGRATION_DIR / "generated"

# Matches the slot-tagged Socket::establish log line emitted by every link
# create/load call site (see source/state-machine/Socket.h).
LOG_LINE_RE = re.compile(
    r"^(?P<ts>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d+): .*"
    r"Socket::establish: slot=(?P<slot>\w+) channel=(?P<channel>\S+) "
    r"role=(?P<role>creator|loader)"
)
TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S.%f"

ROLE_ALIASES = {
    "listener": "listener", "server": "listener",
    "connector": "connector", "client": "connector",
}
ACTION_FOR_ROLE = {"creator": "CREATE", "loader": "LOAD"}


@dataclass(frozen=True)
class LogEvent:
    timestamp: datetime
    action: str
    channel: str
    slot: str


@dataclass(frozen=True)
class Requirement:
    node_id: str
    action: str
    channel: str
    slot: str
    count: int
    # How the spec referred to this node - a literal id, or a role keyword
    # ("connector (each of: dialer, dialer2)") - kept for readable output.
    node_label: str


def default_spec_path(scenario_id: str) -> Path:
    return LINK_EVENTS_DIR / f"{scenario_id}.yaml"


def extract_log_events(log_path: Path) -> List[LogEvent]:
    if not log_path.exists():
        return []
    events = []
    for line in log_path.read_text(errors="replace").splitlines():
        m = LOG_LINE_RE.match(line)
        if not m:
            continue
        events.append(LogEvent(
            timestamp=datetime.strptime(m["ts"], TIMESTAMP_FORMAT),
            action=ACTION_FOR_ROLE[m["role"]],
            channel=m["channel"],
            slot=m["slot"],
        ))
    return events


def load_spec(spec_path: Path) -> Dict[int, list]:
    raw = yaml.safe_load(spec_path.read_text())
    steps_raw = (raw or {}).get("steps")
    if not steps_raw:
        raise ValueError(f"{spec_path}: missing or empty top-level 'steps' mapping")
    steps: Dict[int, list] = {}
    for step_key, events in steps_raw.items():
        step_num = int(step_key)
        if not events:
            raise ValueError(f"{spec_path}: step {step_num} has no events")
        steps[step_num] = events
    return steps


def _resolve_node_ids(node_spec: str, scenario: dict) -> Tuple[List[str], str]:
    """Returns (concrete node ids this event applies to, human-readable label)."""
    alias = ROLE_ALIASES.get(str(node_spec).lower())
    if alias is not None:
        ids = [n["id"] for n in scenario["nodes"] if n["role"] == alias]
        if not ids:
            raise ValueError(f"no '{alias}' nodes in scenario for node spec '{node_spec}'")
        return ids, f"{node_spec} (each of: {', '.join(ids)})"
    known_ids = [n["id"] for n in scenario["nodes"]]
    if node_spec not in known_ids:
        raise ValueError(
            f"unknown node '{node_spec}' (expected one of {sorted(known_ids)} "
            f"or a role keyword: {sorted(ROLE_ALIASES)})"
        )
    return [node_spec], node_spec


def expand_requirements(steps: Dict[int, list], scenario: dict) -> List[Tuple[int, List[Requirement]]]:
    """Returns [(step_num, [Requirement, ...]), ...] sorted by step_num."""
    ordered: List[Tuple[int, List[Requirement]]] = []
    for step_num in sorted(steps):
        step_reqs: List[Requirement] = []
        for raw_event in steps[step_num]:
            action = str(raw_event["action"]).upper()
            if action not in ("CREATE", "LOAD"):
                raise ValueError(
                    f"step {step_num}: invalid action '{raw_event['action']}' "
                    "(expected CREATE or LOAD)"
                )
            node_ids, label = _resolve_node_ids(raw_event["node"], scenario)
            for node_id in node_ids:
                step_reqs.append(Requirement(
                    node_id=node_id,
                    action=action,
                    channel=raw_event["channel"],
                    slot=raw_event.get("slot", "channel"),
                    count=int(raw_event.get("count", 1)),
                    node_label=label,
                ))
        ordered.append((step_num, step_reqs))
    return ordered


def check_link_events(scenario: dict, steps: Dict[int, list], logs_dir: Path) -> Tuple[bool, List[str]]:
    """Verifies that, per-node debug log timestamps, all events required by
    step N occur before any event required by step N+1, matching the
    (node, action, channel, slot) tuples declared in `steps` (see
    LINK_EVENTS_FORMAT.md). Returns (ok, human-readable report lines)."""
    requirement_steps = expand_requirements(steps, scenario)

    # Pool of not-yet-consumed log events per node, earliest first. A log
    # event consumed to satisfy one step's requirement can never also
    # satisfy a later step's requirement for the same tuple.
    pools: Dict[str, List[LogEvent]] = {}
    for node in scenario["nodes"]:
        log_path = logs_dir / node["id"] / f"raceboat_{node['role']}.log"
        pools[node["id"]] = sorted(extract_log_events(log_path), key=lambda e: e.timestamp)

    ok = True
    lines: List[str] = []
    cursor: Optional[datetime] = None
    for step_num, requirements in requirement_steps:
        lines.append(f"Step {step_num}:")
        step_latest: Optional[datetime] = None
        for req in requirements:
            pool = pools.get(req.node_id, [])
            candidates = [
                e for e in pool
                if e.action == req.action and e.channel == req.channel and e.slot == req.slot
                and (cursor is None or e.timestamp > cursor)
            ]
            matched = candidates[:req.count]
            for e in matched:
                pool.remove(e)

            req_ok = len(matched) == req.count
            ok = ok and req_ok
            node_desc = req.node_id if req.node_label == req.node_id else req.node_label
            when = (", ".join(e.timestamp.strftime("%H:%M:%S.%f")[:-3] for e in matched)
                    if matched else "none found")
            lines.append(
                f"  [{'OK' if req_ok else 'MISMATCH'}] {node_desc}: {req.action} "
                f"{req.channel} (slot={req.slot}) x{req.count} -> "
                f"found {len(matched)}/{req.count} (at {when})"
            )
            if matched:
                step_latest = max(step_latest, matched[-1].timestamp) if step_latest else matched[-1].timestamp
        if step_latest is not None:
            cursor = step_latest
    return ok, lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario-id", required=True)
    parser.add_argument("--spec", type=Path, default=None,
                         help="Defaults to scenarios/link_events/<scenario-id>.yaml")
    parser.add_argument("--logs-dir", type=Path, default=None,
                         help="Defaults to generated/<scenario-id>/logs")
    args = parser.parse_args()

    scenario = json.loads((SCENARIOS_DIR / f"{args.scenario_id}.json").read_text())
    spec_path = args.spec or default_spec_path(args.scenario_id)
    if not spec_path.exists():
        print(f"No link-event spec found at {spec_path}; nothing to check.")
        return 0
    logs_dir = args.logs_dir or (GENERATED_DIR / args.scenario_id / "logs")

    steps = load_spec(spec_path)
    ok, lines = check_link_events(scenario, steps, logs_dir)
    print(f"Link-event expectations ({spec_path}):")
    for line in lines:
        print(line)
    print(f"Link-event check: {'PASSED' if ok else 'FAILED'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
