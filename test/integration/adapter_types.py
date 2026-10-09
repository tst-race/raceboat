#!/usr/bin/env python3
"""
adapter_types.py
Shared contract between the scenario orchestrator and per-plugin test adapters.

Each plugin's `test/adapter.py` module implements two functions:
  - `kit_dir(role: str) -> Path`: the built kit directory for this plugin, for
    the given node role (listener/connector). Called by generate_scenario.py's
    build_channel_registry() to discover, purely from manifest.json contents,
    which channel/composition gids this plugin provides - a scenario then
    references a channel/composition gid directly rather than naming a
    plugin_registry.json entry, and plugin discovery (including which OTHER
    plugin(s) a composition's components come from) is fully automatic.
  - `generate_node_contribution(request: NodeRequest) -> NodeContribution`:
    called once per active slot/direction for every node that resolves to
    this plugin as the channel/composition's "owner" (see
    generate_scenario.py's ChannelInfo).
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
    # NOT the plugin's file_path/id. Plain (non-composed/"unified") plugins like
    # racebird ignore this and prefix with their own plugin id instead. Set by
    # the orchestrator to the slot's channel/composition gid whenever that gid
    # is a composition (see generate_scenario.py's ChannelInfo.is_composition);
    # None for a plain unified-plugin channel.
    composition_name: Optional[str] = None
    # None when one shared/bidi contribution fills BOTH this slot's recv and
    # send directions (the common case - e.g. race-cli's single-bidi-link
    # fast path, see ListenStateMachine.cpp's shouldUseSingleBidiLink), in
    # which case the adapter is called once and its address/params apply to
    # both. Set to "recv" or "send" when the scenario splits a slot into two
    # genuinely independent (non-bidi) channels/compositions per direction
    # (see raceboat/source/state-machine/ListenStateMachine.cpp /
    # DialStateMachine.cpp: with distinct recv_channel/send_channel, the
    # listener's "recv" link is the only one with an explicitly
    # created/published address - its "send" link is instead loaded
    # dynamically per-connection from the dialer's hello-message payload
    # (StateDialSendOpen embeds "linkAddress"/"replyChannel" from the
    # dialer's OWN recv link), and symmetrically the dialer's "recv" link is
    # self-created with no preset address, only its "send" link loads a
    # peer-published address. Adapters that support split-direction slots
    # should therefore only publish address_output/cli_flags for the
    # "recv" direction on a listener role and the "send" direction on a
    # connector role, leaving the other direction's contribution address-less.
    direction: Optional[str] = None


@dataclass
class NodeContribution:
    # race-cli --param entries, e.g. {"PluginRacebird.node-id": "..."}.
    params: Dict[str, str]
    # Channel gid to plug into --recv-channel/--send-channel (or final- variants).
    channel_name: str
    # Directory containing this plugin's built artifacts (this plugin's own
    # kit_dir(role) - see module docstring). Any OTHER plugin kits a
    # composition needs (e.g. an encoding implemented elsewhere) are resolved
    # and merged by the orchestrator automatically via manifest introspection
    # (generate_scenario.py's build_channel_registry()) - adapters don't need
    # to know or declare that themselves.
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
