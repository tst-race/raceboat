#!/usr/bin/env python3
"""
adapter_types.py
Shared contract between the scenario orchestrator and per-plugin test adapters.

Each plugin implements `generate_node_contribution(request: NodeRequest) ->
NodeContribution` in a `test/adapter.py` module. The orchestrator dynamically
imports that module (see plugin_registry.json) and calls it once per active
slot for every node in a scenario, merging the resulting contributions.
"""

from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Optional

# Node role within a scenario: "listener" establishes/publishes an address,
# "connector" consumes a listener's published address to reach it.
Role = str

# Which race-cli flag pair this contribution fills:
#   "channel" -> --recv-channel/--send-channel (single-plugin modes)
#   "initial" -> --recv-channel/--send-channel (bootstrap's initial phase)
#   "final"   -> --final-recv-channel/--final-send-channel (bootstrap's final phase)
Slot = str


@dataclass
class NodeRequest:
    role: Role
    node_id: str
    ip: str
    slot: Slot
    # Outputs already published by other nodes this adapter depends on, keyed
    # by the publishing node_id (e.g. a connector reads its listener's entry).
    peer_context: Dict[str, dict] = field(default_factory=dict)
    # For composition-based plugins (see raceboat/source/plugin-loading/Config.cpp:
    # "ensure channelParameter.plugin matches plugin/composition id"), --param
    # entries must be prefixed with the manifest's composition id, e.g.
    # "skyhookBasicComposition.region=..." (raceboat/README.md / skyhook/README.md),
    # NOT the plugin's file_path/id. Plain (non-composed) plugins like racebird
    # ignore this and prefix with their plugin id instead. The scenario file
    # supplies this when a node's slot uses a composition-based plugin.
    composition_name: Optional[str] = None


@dataclass
class NodeContribution:
    # race-cli --param entries, e.g. {"PluginRacebird.node-id": "..."}.
    params: Dict[str, str]
    # Channel gid to plug into --recv-channel/--send-channel (or final- variants).
    channel_name: str
    # Directory containing this plugin's built artifacts, merged into a shared kits/ dir.
    kit_dir: Path
    # Address info to publish for peer nodes to consume (e.g. a listener's link cert).
    address_output: Optional[dict] = None
    # Whether this node's race-cli command needs a peer's address_output (e.g. --send-address).
    needs_peer_address: bool = False
    # Extra docker-compose services this plugin requires (e.g. a whiteboard sidecar),
    # keyed by service name so the orchestrator can dedup across nodes/plugins.
    sidecar_services: Dict[str, dict] = field(default_factory=dict)
    # Direct race-cli flags outside the --param namespace, e.g. {"send-address": "{...json...}"}.
    cli_flags: Dict[str, str] = field(default_factory=dict)
