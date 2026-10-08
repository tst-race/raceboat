# Testing a Newly Developed Plugin

Audience: an agent (or developer) that has just written or is reviewing a new
raceboat comms plugin and needs to add it to the integration test framework
under `raceboat/test/integration/`. This doc is deliberately procedural -
follow the steps in order. Background/rationale lives in
[README.md](README.md); this file is the task checklist.

## 0. Core model you must understand before writing anything

- A scenario file (`scenarios/<id>.json`) assigns each node a **channel or
  composition gid** per slot (`"channel"` for plain `client-connect` mode,
  `"initial"`/`"final"` for `bootstrap-connect` mode) - e.g. `"obfs4"`,
  `"twoSixIndirectComposition"`, `"skyhookBasicComposition"`. It does **NOT**
  reference a `plugin_registry.json` key directly.
- `generate_scenario.py` figures out which plugin(s) implement a given gid by
  scanning the built `manifest.json` of **every** plugin listed in
  `plugin_registry.json` (see `build_channel_registry()`). This happens
  automatically - you do not maintain a mapping anywhere.
- A gid is either:
  - a **plain/unified channel** (manifest `plugins[].channels`, e.g.
    racebird's `"obfs4"`) - implemented entirely by one plugin, or
  - a **composition** (manifest `compositions[]`, e.g. decomposed-exemplars'
    `"twoSixIndirectComposition"`) - built from a transport + usermodel +
    encoding(s), each of which may be implemented by a **different** plugin.
    If your composition's encoding (or usermodel, or transport) is
    implemented by a plugin OTHER than the one declaring the composition,
    that is handled automatically too, AS LONG AS that other plugin is also
    listed in `plugin_registry.json` - see §4.

Run this at any time to see what's currently resolvable and by whom (do this
first if you're unsure whether a gid you're reading in an existing scenario
belongs to one plugin or several):

```bash
cd raceboat/test/integration
python3 generate_scenario.py --list-channels listener
python3 generate_scenario.py --list-channels connector
```

(Output differs by role because some plugins - e.g. skyhook - ship a
different manifest/transport per side.)

## 1. Prerequisites

- The plugin is built and its kit artifacts exist on disk, e.g.
  `<plugin-dir>/kit/artifacts/linux-<arch>-server/Plugin<Name>/manifest.json`
  (and a `-client` variant if client/server use different artifacts - see §5
  for when you need this).
- You know the plugin's channel gid (unified) or composition id(s)
  (decomposed) - check the plugin's own `manifest.json` under
  `"channel_properties"` / `"compositions"`.

## 2. Implement `test/adapter.py`

Create `<plugin-dir>/test/adapter.py`. Two functions are required:

```python
#!/usr/bin/env python3
import json
import platform
import sys
from pathlib import Path

try:
    from adapter_types import NodeContribution, NodeRequest
except ImportError:
    _integration_dir = Path(__file__).resolve().parents[2] / "raceboat" / "test" / "integration"
    sys.path.insert(0, str(_integration_dir))
    from adapter_types import NodeContribution, NodeRequest  # noqa: E402


def _detect_host_architecture() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        return "x86_64"
    if machine in {"arm64", "aarch64"}:
        return "arm64-v8a"
    raise ValueError(f"Unsupported host architecture: {platform.machine()}")


def kit_dir(role: str) -> Path:
    # REQUIRED. Called before any scenario is known, purely to locate and
    # parse this plugin's manifest.json for channel/composition discovery
    # (see build_channel_registry() in generate_scenario.py). If your plugin
    # builds the SAME artifacts for both roles, ignore `role` (see racebird/
    # decomposed-exemplars adapters). If client vs server differ (e.g. a
    # plugin with an account-holder/public-user split like skyhook), branch
    # on `role == "listener"` vs `"connector"`.
    plugin_root = Path(__file__).resolve().parents[1]
    return (
        plugin_root / "kit" / "artifacts"
        / f"linux-{_detect_host_architecture()}-server"
        / "PluginYourName"
    )


def generate_node_contribution(request: NodeRequest) -> NodeContribution:
    # REQUIRED. Called once per (node, slot, direction) that resolves to this
    # plugin as the channel/composition's OWNER (see ChannelInfo.plugin_dirs[0]
    # in generate_scenario.py) - i.e. your adapter is never called for a
    # composition you merely contribute a component to but don't declare.
    #
    # request.composition_name is the composition gid (None for a plain
    # unified channel) - --param entries must be prefixed with it, not your
    # plugin's own id, when it's set (see raceboat/source/plugin-loading/
    # Config.cpp: "ensure channelParameter.plugin matches plugin/composition id").
    prefix = request.composition_name or "PluginYourName"

    contribution = NodeContribution(
        params={f"{prefix}.some-param": "value"},
        channel_name=request.composition_name or "yourChannelGid",
        kit_dir=kit_dir(request.role),
    )

    # request.direction is None unless this slot was SPLIT into two different
    # channels/compositions per direction (see §6). When set, only the
    # listener's "recv" and the connector's "send" direction get an explicit
    # address - skip address logic entirely for the other direction (see
    # adapter_types.NodeRequest.direction's docstring for why).
    is_dynamic_direction = (
        request.direction == "send" if request.role == "listener"
        else request.direction == "recv" if request.role == "connector"
        else False
    )
    if is_dynamic_direction:
        return contribution

    if request.role == "listener":
        address = {"...": "..."}  # whatever your transport needs to publish
        contribution.address_output = address
        contribution.cli_flags["recv-address"] = json.dumps(address)
    elif request.role == "connector":
        contribution.needs_peer_address = True
        listener_address = next(iter(request.peer_context.values()), None)
        if listener_address is None:
            raise ValueError("connector needs a listener address but none was provided")
        contribution.cli_flags["send-address"] = json.dumps(listener_address)

    return contribution
```

Full field-by-field contract: `adapter_types.py` (`NodeRequest`,
`NodeContribution`). Real examples: `racebird/test/adapter.py` (plain unified
channel), `decomposed-exemplars/test/adapter.py` (composition, direction-aware
split support), `skyhook/test/adapter.py` (role-dependent `kit_dir`,
untested/documents the AWS-credential limitation - read its module docstring
before copying it).

## 3. Register the plugin

Add one line to `raceboat/test/integration/plugin_registry.json`:

```json
{
  "your-plugin": "../../../your-plugin"
}
```

This is ONLY used so `generate_scenario.py` knows which directories to scan
for manifests/adapters - it is never referenced from a scenario file. It is
safe to add a plugin here even if it isn't built yet in this environment:
`build_channel_registry()` prints a `[channel registry] skipping '<plugin>'
... not built?` warning and continues; it only becomes an error if a scenario
you run actually needs a channel/component that plugin would have provided.

## 4. The cross-plugin composition case

If your plugin is a **decomposed** plugin (transport/usermodel/encoding
components, no plain `channels` entry) and its `compositions[]` entry
references a component name your plugin does **not** itself implement (e.g.
you only ship a transport + usermodel and want to reuse an existing encoding
like `"base64"` or `"noop"` from decomposed-exemplars), you do **NOT** need to
do anything special beyond §3: as long as BOTH plugins are listed in
`plugin_registry.json`, `build_channel_registry()` resolves the missing
component from whichever registered plugin's manifest declares it (see
`raceboat/source/plugin-loading/PluginLoader.cpp`: every manifest under
`--dir` is parsed and all transports/usermodels/encodings are merged into one
global namespace before a composition is resolved).

Concretely: skyhook's manifest declares compositions referencing encoding
`"noop"`, which it does not implement - decomposed-exemplars does. Adding
`"skyhook": "../../../skyhook"` to `plugin_registry.json` (next to the
already-present `"decomposed-exemplars"` entry) is the ONLY wiring needed for
`generate_scenario.py --list-channels listener` to show:

```
  skyhookBasicComposition (composition) -> plugins ['skyhook', 'decomposed-exemplars']
```

Verify your own cross-plugin composition resolves correctly the same way
before writing a scenario - if a needed component's plugin isn't registered
yet, `build_channel_registry()` raises a clear error naming the missing
component(s) and which plugins it did scan, e.g.:

```
ValueError: composition 'yourComposition' (role=listener) needs component(s)
['someEncoding'] but no plugin registered in plugin_registry.json provides
them (scanned: ['decomposed-exemplars', 'racebird', 'your-plugin']). Add the
plugin that implements ['someEncoding'] to plugin_registry.json.
```

If this happens, find which plugin implements that component (grep its
manifest.json for the name under `"encodings"`/`"transports"`/`"usermodels"`)
and add it to `plugin_registry.json`.

## 5. Role-dependent kit artifacts (client vs server differ)

Skip this section if your plugin builds identical artifacts for both roles
(the common case - see racebird/decomposed-exemplars).

If your plugin builds DIFFERENT artifacts/manifests per side (e.g. an
account-holder vs public-user split like skyhook, or any plugin that needs a
privileged credential only on one side), `kit_dir(role)` must branch:

```python
def kit_dir(role: str) -> Path:
    side = "server" if role == "listener" else "client"
    return plugin_root / "kit" / "artifacts" / f"linux-{arch}-{side}" / "PluginYourName"
```

`build_channel_registry()` is built separately per role, so the
listener-side and connector-side manifests can declare the SAME composition
id with genuinely different transport components (this is exactly how
skyhook's `skyhookBasicComposition` works: listener resolves to the
AccountHolder transport, connector to the PublicUser transport).

## 6. Writing the scenario file

Create `scenarios/your-plugin-client-connect.json`:

```json
{
  "id": "your-plugin-client-connect",
  "mode": "client-connect",
  "wait_time": 15,
  "network": {"name": "your-plugin-network", "subnet": "10.50.1.0/24"},
  "nodes": [
    {"role": "listener", "id": "listener", "ip": "10.50.1.3", "slots": {"channel": "yourChannelGid"}},
    {"role": "connector", "id": "dialer", "ip": "10.50.1.2", "slots": {"channel": "yourChannelGid"}}
  ]
}
```

For `bootstrap-connect` mode, use `"initial"`/`"final"` slots instead of
`"channel"` - see `scenarios/bootstrap-decomposed-decomposed.json` for the
simplest example.

### Splitting upstream/downstream into different channels

A slot value can be `{"recv": "gidA", "send": "gidB"}` instead of a plain
string, when the two directions should use genuinely different
channels/compositions - either two compositions of the SAME plugin (e.g.
`twoSixIndirectComposition` for recv, `twoSixIndirectCompositionReactive` for
send - see `scenarios/decomposed-client-connect-dual-composition.json`) or
two entirely DIFFERENT plugins (e.g. `obfs4` for recv, `twoSixIndirectComposition`
for send - see any `bootstrap-mixed-*.json` scenario). Both nodes must use
MATCHING, swapped assignments:

```json
"slots": {"channel": {"recv": "gidA", "send": "gidB"}}   // listener
"slots": {"channel": {"recv": "gidB", "send": "gidA"}}   // connector
```

Your adapter must be direction-aware for this to work correctly (see
`NodeRequest.direction` in §2 and `decomposed-exemplars/test/adapter.py` for
the reference implementation) - otherwise the listener/connector will each
try to publish TWO conflicting addresses and the handshake will hang.

### Forcing a link direction (`linkDirectionOverrides`)

Keyed by **channel/composition gid**, not plugin name:

```json
"linkDirectionOverrides": {
  "yourChannelGid": {"channel": "LD_LOADER_TO_CREATOR"}
}
```

See `generate_scenario.py`'s `_apply_link_direction_overrides()` docstring
for when this is (and isn't) meaningful for your channel's `sendType`.

## 7. Run it

```bash
cd raceboat/test/integration

# Rebuild your plugin and run the new scenario in one step:
python3 build-test.py --plugin-dir ../../your-plugin --rebuild-plugin --integration-test-args --scenario-id your-plugin-client-connect --image-tag latest

# Or, once kits are built, just regenerate + run directly:
python3 run_scenario.py --scenario-id your-plugin-client-connect --image-tag latest
```

Read the printed `Resolved channels/compositions:` block first - it lists
every gid this run resolved and the plugin(s) pulled in for it. If something
looks wrong (wrong plugin, missing donor plugin, etc.), fix `plugin_registry.json`
or your adapter before chasing a runtime failure.

## 8. Troubleshooting checklist

- `unknown channel/composition '<gid>'` - typo in the scenario, or the
  plugin that declares it isn't in `plugin_registry.json` yet. Run
  `--list-channels <role>` to see what IS resolvable.
- `composition '<gid>' ... needs component(s) [...] but no plugin registered
  ... provides them` - see §4; register the plugin that implements the named
  component(s).
- `no manifest.json found at <path> (role=...) - is it built?` (from
  `build_channel_registry`, hard-fails only if a scenario actually needs that
  plugin) - build the plugin's kit artifacts first.
- Handshake hangs with no errors in a split-direction (`{"recv": ..., "send": ...}`)
  scenario - almost always a non-direction-aware adapter publishing an
  address for BOTH directions. See §6's split-direction note.
- A scenario's own `INTEGRATION TEST PASSED/FAILED` line is the authoritative
  functional result. `run_scenario.py`'s overall exit code also folds in the
  (separately flaky) `scenarios/link_events/<id>.yaml` ordering check if one
  exists for your scenario id - don't write one until the scenario's basic
  message exchange passes reliably first.
