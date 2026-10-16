# Raceboat Integration Test Framework

This directory contains generic infrastructure for testing raceboat plugins end-to-end using Docker Compose.

> **Testing a newly developed plugin?** See [TESTING_NEW_PLUGINS.md](TESTING_NEW_PLUGINS.md)
> for a step-by-step walkthrough, including plugins whose composition needs
> components (e.g. an encoding) implemented by a different plugin.

## Quick Start

**For most development workflows**, use the `build-test.py` orchestrator which handles rebuilds and testing:

```bash
# Test without rebuilding (fastest)
python3 build-test.py --plugin-dir ../../racebird

# Rebuild plugin only, then test
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin

# Rebuild everything, then test
python3 build-test.py --plugin-dir ../../racebird --rebuild-all

# Works with any plugin directory (relative or absolute path)
python3 build-test.py --plugin-dir /path/to/my-custom-plugin --rebuild-plugin

# See full options
python3 build-test.py --help
```

See [BUILD_TEST_GUIDE.md](BUILD_TEST_GUIDE.md) for detailed build-test workflow documentation.

> **Image tag gotcha:** `build-test.py`'s rebuild stages always update the
> `:latest` docker tags, but the test step (`run_scenario.py`) defaults to
> `--image-tag main`. After any `--rebuild-raceboat*`/`--rebuild-all`, forward
> the matching tag explicitly, e.g.:
> `python3 build-test.py --plugin-dir ../../racebird --rebuild-all --integration-test-args --image-tag latest`

**For direct test execution against an already-built scenario** (no rebuild), use `run_scenario.py`:

```bash
# Run one of the predefined scenarios under scenarios/*.json
python3 run_scenario.py --scenario-id racebird-client-connect --image-tag latest

# List available scenarios
ls scenarios/*.json
```

See [Scenario-Based Testing](#scenario-based-testing) below for details. For a
plugin that still uses a hand-written `docker-compose.yml` (i.e. hasn't
migrated to `test/adapter.py`), call `run-integration-test.py` directly instead:

```bash
python3 run-integration-test.py --compose-file /path/to/docker-compose.yml
```

## Overview

The integration test validates bidirectional message delivery through raceboat's socket modes:

- **Server mode (`--server-connect`)**: raceboat connects TO localhost:7777 when receiving data via the raceboat channel
- **Client mode (`--client-connect`)**: raceboat listens ON localhost:9999 for application connections

The test sends a message through the client socket, which triggers raceboat to deliver it through the raceboat channel to the server, which then connects back to a test stub.

## Test Flow

```
[tcp-stub-client]  →  [raceboat:9999]  →  raceboat Channel  →  [raceboat]  →  [tcp-stub-server:7777]
      connects           --client-connect                      --server-connect      listening
```

Detailed sequence:

1. Start server stub listening on port 7777
2. Client stub connects to port 9999 (where raceboat --client-connect listens)
3. Client stub sends "hello from client"
4. raceboat (client) receives the message and sends it through the raceboat channel
5. raceboat (server) receives the message and connects to localhost:7777
6. Server stub receives "hello from client" and replies "hello from server"
7. raceboat (server) receives the reply and sends it back through the raceboat channel
8. raceboat (client) receives the reply and delivers it to the client stub
9. Both stubs exit with success codes

## Scenario-Based Testing

Modern plugins - and every multi-node or `bootstrap-connect` test - are driven
by a JSON scenario file under `scenarios/*.json` describing the network
topology (node roles, IPs, and which **channel or composition gid** fills
each "slot", e.g. `"obfs4"`, `"twoSixIndirectComposition"`,
`"skyhookBasicComposition"` - NOT a `plugin_registry.json` key). Which
plugin(s) implement that gid is resolved automatically by scanning every
plugin in `plugin_registry.json`'s built `manifest.json`
(`generate_scenario.py`'s `build_channel_registry()` - see
[TESTING_NEW_PLUGINS.md](TESTING_NEW_PLUGINS.md) for the full write-up,
including compositions that span more than one plugin). The scenario is
rendered into a docker-compose file by `generate_scenario.py` (which calls
the owning plugin's `test/adapter.py`) and executed by `run_scenario.py`:

```bash
python3 run_scenario.py --scenario-id <scenario-id> --image-tag <tag>
```

- `--scenario-id` (required): name of a file under `scenarios/` (without `.json`)
- `--image-tag` (default: `main`): the `raceboat-runtime` tag to test against
- `--wait-time` (optional): overrides the scenario's own `wait_time` (RACE channel establishment delay)

To run every scenario under `scenarios/*.json` in one command (each in its own
subprocess, with a pass/fail summary printed at the end), use
`run_all_scenarios.py`:

```bash
python3 run_all_scenarios.py --image-tag <tag>

# Only run a subset, or exclude some
python3 run_all_scenarios.py --image-tag <tag> --only racebird-client-connect bootstrap-racebird-racebird
python3 run_all_scenarios.py --image-tag <tag> --skip bootstrap-decomposed-decomposed-final-c2l

# Stop at the first failure
python3 run_all_scenarios.py --image-tag <tag> --fail-fast
```

Currently available scenarios:
- `racebird-client-connect`, `decomposed-client-connect`,
  `decomposed-client-connect-multi-client` - single-channel `client-connect`/`server-connect` mode
- `decomposed-client-connect-dual-composition` - plain `client-connect` mode,
  but the "channel" slot uses TWO DIFFERENT compositions of the same plugin
  (`twoSixIndirectComposition` for recv, `twoSixIndirectCompositionReactive`
  for send) instead of one shared bidi channel - demonstrates upstream and
  downstream using genuinely different channels/compositions; see
  [TESTING_NEW_PLUGINS.md](TESTING_NEW_PLUGINS.md)
- `bootstrap-decomposed-racebird`, `bootstrap-racebird-multi-client`,
  `bootstrap-racebird-racebird-multi-client`, `bootstrap-racebird-decomposed-multi-client`,
  `bootstrap-decomposed-decomposed-multi-client` - `bootstrap-connect` mode (one listener plus one
  or more connector nodes, each with an "initial" and "final" channel/composition slot)
- `decomposed-client-connect-multi-client-long` - same 2-connector/1-listener
  topology as `decomposed-client-connect-multi-client` (both connectors
  legitimately share the SAME listener-created link address), but exchanges
  10 sequential messages per direction using the `tcp-stub-*-multimsg.py`
  stubs (see optional scenario keys `messages`/`stub_client`/`stub_server`
  below) instead of just one, to stress-test sustained multi-client traffic
  sharing one address
- `decomposed-client-connect-multi-client-forced-l2c` - same topology/stub
  setup as the `-long` scenario above, but forces twoSixIndirect's "channel"
  slot to `LD_LOADER_TO_CREATOR` via `linkDirectionOverrides`. twoSixIndirect
  posts and fetches using a single `address.hashtag` per Link (see
  `decomposed-exemplars/source/transport/Link.cpp`), so its native `LD_BIDI`
  mode shares ONE hashtag for both directions; forcing this override makes
  each direction create its own separate link/hashtag instead
- `racebird-client-connect-timeout-zero` - single listener/connector,
  `"timeout": 0`, used by `test-timeout-zero-teardown.py` (see below) to
  verify race-cli's immediate-teardown-on-local-disconnect behavior

Each run's generated compose file, merged plugin kits, and per-node
`raceboat_*.log` files are written to `generated/<scenario-id>/`. Containers
(e.g. a whiteboard sidecar) can leave root-owned files there, so if a rerun's
cleanup fails, remove it manually first:

```bash
docker compose -f generated/<scenario-id>/docker-compose.yml down
sudo rm -rf generated/<scenario-id>
```

For `bootstrap-connect` scenarios, `run_scenario.py` also prints a link-topology
summary (see `link_topology.py`) verifying the listener creates, and each
connector loads, the expected number of "final"-slot links.

Any scenario can also have an optional, hand-written
`scenarios/link_events/<scenario-id>.yaml` describing the expected order of
link `CREATE`/`LOAD` events (which node, which channel, which slot) - see
[LINK_EVENTS_FORMAT.md](LINK_EVENTS_FORMAT.md). Unlike `link_topology.py`,
which hardcodes the "listener creates, connector loads" assumption in Python,
this is a declarative, per-scenario spec, so it correctly describes
role-reversed scenarios too (e.g. `linkDirectionOverrides`). If a matching
spec file exists, `run_scenario.py` runs `link_events.py` against it
automatically and folds a FAILED result into the overall exit code.

## Files

### Scripts
- **`build-test.py`**: Rebuild-and-test orchestrator (raceboat SDK + plugin builds, then runs a scenario or legacy compose test) - see [BUILD_TEST_GUIDE.md](BUILD_TEST_GUIDE.md)
- **`run_scenario.py`**: Generates a scenario's docker-compose.yml and runs it through `run-integration-test.py`
- **`run_all_scenarios.py`**: Runs every (or a selected subset of) `scenarios/*.json` scenario via `run_scenario.py` and prints a pass/fail summary
- **`generate_scenario.py`**: Renders a `scenarios/*.json` topology into a docker-compose file. Scans every plugin in `plugin_registry.json`'s built `manifest.json` to automatically resolve each slot's channel/composition gid to the plugin(s) that implement it (`build_channel_registry()`), then calls the owning plugin's `test/adapter.py`. Supports `--list-channels listener|connector` to print every resolvable channel/composition gid and which plugin(s) it needs, without generating a scenario
- **`link_topology.py`**: Parses debug logs and prints observed link create/load counts per channel (informational only)
- **`link_events.py`**: Checks debug logs against a developer-authored `scenarios/link_events/<id>.yaml` link-event ordering spec - see [LINK_EVENTS_FORMAT.md](LINK_EVENTS_FORMAT.md)
- **`run-integration-test.py`**: Generic test orchestrator (plugin-agnostic) that drives an already-generated docker-compose.yml. Supports optional `--server-stub-path`/`--client-stub-path`/`--messages` args to swap in a different stub pair and/or run multiple sequential round-trip messages (all default to today's single-message behavior)
- **`test-timeout-zero-teardown.py`**: Standalone test (not scenario-JSON-driven, since it must monitor container lifecycle rather than just message content) verifying that `--timeout 0` tears down the conduit and exits race-cli within 10s of the local application disconnecting
- **`tcp-stub-server.py`**: Server-side test stub that listens on port 7777
- **`tcp-stub-client.py`**: Client-side test stub that connects to port 9999
- **`tcp-stub-server-multimsg.py`** / **`tcp-stub-client-multimsg.py`**: Like the above, but exchange N sequential round-trip messages (`msgI` indices, strict per-connection ordering/id checks) over one persistent connection - used by scenarios that set the `messages`/`stub_server`/`stub_client` keys (e.g. `decomposed-client-connect-multi-client-long`), and by `test-timeout-zero-teardown.py`
- **`adapter_types.py`**: `NodeRequest`/`NodeContribution` dataclasses - the contract every `test/adapter.py` implements
- **`plugin_registry.json`**: Maps plugin name -> plugin directory, so `generate_scenario.py` can scan each plugin's manifest.json and import its `test/adapter.py`. Scenario files reference channel/composition gids directly, not this registry's keys - see `generate_scenario.py --list-channels`
- **`example-docker-compose.yml`**: Template for legacy (non-adapter) plugin compose files

### Data
- **`scenarios/*.json`**: Declarative topology definitions (nodes, roles, channel/composition slots) consumed by `run_scenario.py`
- **`scenarios/link_events/<scenario-id>.yaml`**: Optional per-scenario link-event ordering spec consumed by `link_events.py` - see [LINK_EVENTS_FORMAT.md](LINK_EVENTS_FORMAT.md)
- **`generated/<scenario-id>/`**: Per-run output (compose file, merged kits, logs) - safe to delete between runs

- **`README.md`**: This file
- **`TESTING_NEW_PLUGINS.md`**: Step-by-step walkthrough for writing a scenario that tests a newly developed plugin, including cross-plugin compositions

## Usage

### Basic Usage

```bash
python3 run-integration-test.py --compose-file /path/to/docker-compose.yml
```

### With Options

```bash
python3 run-integration-test.py \
    --compose-file docker-compose.yml \
    --wait-time 15 \
    --name "My Plugin Test" \
    --server-container myserver \
    --client-container myclient
```

## Options

- `--compose-file <path>`: Path to docker-compose.yml (required)
- `--wait-time <seconds>`: Time to wait for raceboat channel establishment (default: 10)
- `--name <name>`: Test name for display (default: raceboat Integration Test)
- `--server-container <name>`: Server container name (default: listener)
- `--client-container <name>`: Client container name (default: dialer)
- `--additional-clients <name1> <name2> ...`: Additional client containers for multi-client testing (optional)
- `--clear-logs`: Clear log directories before running test
- `--log-dirs <dir1> <dir2> ...`: Log directories to clear (relative to compose file directory)

### Clearing Logs Before Tests

To automatically clear logs before each test run:

```bash
# Auto-detect log directories (any directory ending with '-logs')
python3 run-integration-test.py \
    --compose-file docker-compose.yml \
    --clear-logs

# Or specify log directories explicitly
python3 run-integration-test.py \
    --compose-file docker-compose.yml \
    --clear-logs \
    --log-dirs server-logs client-logs client2-logs
```

When `--clear-logs` is used without `--log-dirs`, the script automatically detects and clears any directories ending with `-logs` in the same directory as the docker-compose file. This ensures each test run starts with fresh log files, making it easier to debug issues.

**Note:** The `build-test.py` orchestrator automatically enables log clearing.

### Multi-Client Testing

To test multiple simultaneous client connections, use the `--additional-clients` option:

```bash
python3 run-integration-test.py \
    --compose-file docker-compose.yml \
    --client-container dialer \
    --additional-clients dialer2 dialer3
```

This will:
- Start all specified client containers
- Verify the server can handle multiple simultaneous connections
- Ensure bidirectional communication works for all clients
- Report success only if all clients successfully exchange messages with the server

The test validates that:
1. Each client can send messages to the server
2. The server receives messages from all clients
3. The server sends replies to all clients
4. Each client receives the reply from the server

## Docker Compose Requirements

Your docker-compose.yml must have:

### 1. Container Names
- Default: `listener` and `dialer`
- Or use `--server-container` and `--client-container` flags

### 2. raceboat Modes
- **Server**: Must use `--server-connect` mode
- **Client**: Must use `--client-connect` mode

### 3. Python 3
- Both containers must have python3 available

### 4. Network Configuration
- Containers must be able to communicate via the raceboat channel
- Server must have plugin kits with correct link addresses

### 5. Volume Mounts (recommended)
- Map plugin kits directory: `./kits:/server-kits` 
- Map log directory: `./server-logs:/tmp` and `./client-logs:/tmp`

See `example-docker-compose.yml` for a complete template.

## Creating a Plugin Integration Test

### Recommended: adapter-based plugin

Current convention (see `racebird/test/adapter.py` or
`decomposed-exemplars/test/adapter.py` for real examples): a plugin owns only
a `test/adapter.py` module - no plugin-specific `docker-compose.yml`,
`setup.py`, or wrapper script needed.

#### Step 1: Implement `test/adapter.py`

```python
from adapter_types import NodeContribution, NodeRequest  # see plugin_registry.json for import path

def kit_dir(role: str) -> Path:
    return script_dir / ".." / "kit" / "artifacts" / "linux-x86_64-server" / "PluginYourPlugin"

def generate_node_contribution(request: NodeRequest) -> NodeContribution:
    return NodeContribution(
        params={"YourPlugin.node-id": "..."},
        channel_name="yourChannelGid",
        kit_dir=kit_dir(request.role),
        address_output={"...": "..."} if request.role == "listener" else None,
        needs_peer_address=request.role == "connector",
    )
```

`kit_dir(role)` is called up front (before any scenario-specific info is
known) to scan this plugin's manifest.json and discover which channel/
composition gid(s) it provides - see `generate_scenario.py`'s
`build_channel_registry()`. `generate_node_contribution` is then called once
per active "slot" (`channel`/`initial`/`final`) for every node in the
scenario whose slot resolves to this plugin; see `adapter_types.py` for the
full field-by-field contract.

#### Step 2: Register the plugin

Add an entry to `plugin_registry.json`:

```json
{
  "your-plugin": "../../../your-plugin"
}
```

This is only used to discover plugins for manifest scanning - scenario files
reference the channel/composition gid itself (e.g. `"yourChannelGid"`), not
this registry key (see Step 3). If your plugin is a decomposed/composition
plugin whose composition references a component (e.g. an encoding)
implemented by a DIFFERENT already-registered plugin, that's resolved
automatically too - no extra bookkeeping needed, as long as both plugins are
listed here. Run `python3 generate_scenario.py --list-channels listener` (or
`connector`) to see every channel/composition gid currently resolvable and
which plugin(s) it needs - useful both when writing a new scenario and when
reading an existing one that references a gid you don't recognize.

#### Step 3: Add a scenario

Add `scenarios/your-plugin-client-connect.json` describing a listener and one
or more connector nodes with `"slots": {"channel": "yourChannelGid"}` (or
`"initial"`/`"final"` for `bootstrap-connect` scenarios) - using the
channel/composition gid itself, not the plugin_registry.json key. A slot can
also be `{"recv": "gidA", "send": "gidB"}` to use two different
channels/compositions for the two directions (e.g. a plain client-connect
test where upstream and downstream use different compositions of the same
plugin, or entirely different plugins).

#### Step 4: Run it

```bash
python3 build-test.py --plugin-dir ../../your-plugin --rebuild-plugin
# or, once built:
python3 run_scenario.py --scenario-id your-plugin-client-connect
```

See [TESTING_NEW_PLUGINS.md](TESTING_NEW_PLUGINS.md) for a step-by-step,
agent-oriented walkthrough of this process, including the case where your
plugin's composition needs a component (e.g. an encoding) implemented by a
different, already-registered plugin.

### Legacy: hand-written docker-compose.yml

Plugins that haven't migrated to `test/adapter.py` still work via a
plugin-owned `docker-compose.yml` + `setup.py`, run directly through
`run-integration-test.py`.

#### Step 1: Create plugin-specific docker-compose.yml

Create a `docker-compose.yml` with your plugin's specific parameters:

```yaml
services:
  listener:
    image: ghcr.io/tst-race/raceboat/raceboat-runtime:latest
    command: >
      race-cli -m --server-connect --debug
      --logto /tmp/raceboat_server.log
      --param YourPlugin.param1="value1"
      --param YourPlugin.param2="value2"
  
  dialer:
    image: ghcr.io/tst-race/raceboat/raceboat-runtime:latest
    command: >
      race-cli -m --client-connect --debug
      --logto /tmp/raceboat_client.log
      --send-address="YOUR_LINK_ADDRESS"
```

This compose file will be invoked by the integration-test script to run the test. The dialer and listener containers must exist and use the specified images (or images built on top of them), and the raceboat command must be invoked. However, the arguments to the race-cli command (e.g. the send address, the params, etc.) will be customized to the plugin being tested. Additionally, for channels reliant on additional self-hosted services (e.g. if testing a "localized" version of an email channel that relies on the existence of an email server) those can be added to the docker-compose file to support a fully automated test.

#### Step 2: Create setup script

Create a `setup.py` that prepares plugin kits. This just copies the built plugin kits into a test directory that will be volume mounted by the test client and test server containers, to be used at runtime.

```python
#!/usr/bin/env python3
import shutil
from pathlib import Path

script_dir = Path(__file__).parent.resolve()
source = script_dir / '..' / 'kit' / 'artifacts' / 'linux-arm64-v8a-server' / 'PluginYourPlugin'
dest = script_dir / 'kits' / 'PluginYourPlugin'

if dest.exists():
    shutil.rmtree(dest)
dest.parent.mkdir(parents=True, exist_ok=True)
shutil.copytree(source, dest)
```

#### Step 3: Create wrapper integration test script

Create an `integration-test.py` that ties everything together. This handles invoking the setup script, clearing logs, and then invoking the raceboat integration test to test bidirectional communication over the raceboat plugin.

```python
#!/usr/bin/env python3
import os
import sys
import subprocess
import shutil
from pathlib import Path

script_dir = Path(__file__).parent.resolve()
raceboat_dir = script_dir / '..' / '..' / 'raceboat'

# Setup plugin artifacts
subprocess.run([sys.executable, str(script_dir / 'setup.py')], check=True)

# Clear logs
for log_dir in [script_dir / 'server-logs', script_dir / 'client-logs']:
    if log_dir.exists():
        for item in log_dir.iterdir():
            if item.is_file():
                item.unlink()
            elif item.is_dir():
                shutil.rmtree(item)

# Run generic test
os.execv(sys.executable, [
    sys.executable,
    str(raceboat_dir / 'test' / 'integration' / 'run-integration-test.py'),
    '--compose-file', str(script_dir / 'docker-compose.yml'),
    '--wait-time', '10',
    '--name', 'Your Plugin Integration Test'
])
```

#### Step 4: Run the test

```bash
cd your-plugin/scripts
./integration-test.py
# or
python3 integration-test.py
```

## Networking: IPv4 vs IPv6

The TCP stubs automatically handle both IPv4 and IPv6:

- **Server stub**: Tries IPv6 dual-stack first, falls back to IPv4
- **Client stub**: Tries IPv4 first, falls back to IPv6
- This ensures compatibility across different container configurations

If you encounter "Address family not supported" errors, it means your containers don't support IPv6. The stubs will automatically fall back to IPv4.

## Exit Codes

### Server Stub (`tcp-stub-server.py`)
- `0`: Success - received expected message
- `1`: Failure - unexpected data received
- `2`: Failure - timeout waiting for connection/data
- `3`: Failure - exception or bind error

### Client Stub (`tcp-stub-client.py`)
- `0`: Success - received expected reply
- `1`: Failure - unexpected data received
- `2`: Failure - could not connect after retries
- `3`: Failure - timeout
- `4`: Failure - exception

### Test Runner (`run-integration-test.sh` / `run-integration-test.py`)
- `0`: Test passed (both stubs succeeded)
- `1`: Test failed (at least one stub failed)

## Troubleshooting

### Test fails immediately
- Check that containers started: `docker ps`
- Check raceboat logs: `docker logs listener` and `docker logs dialer`
- Verify plugin kits are mounted correctly

### Client stub times out
- Increase `--wait-time` to allow more time for raceboat channel establishment
- Check that server has correct link address in `--send-address`
- Verify network connectivity between containers

### Server stub times out
- Check that raceboat is using `--server-connect` mode
- Verify client sends data to trigger the server connection
- Check server raceboat logs for connection errors

### "Address family not supported"
- This is expected if containers don't support IPv6
- The stubs automatically fall back to IPv4
- No action needed unless both IPv4 and IPv6 fail

### `PermissionError` when re-running a scenario
- Containers (e.g. the whiteboard sidecar) leave root-owned files under
  `generated/<scenario-id>/`, which `generate_scenario.py` can't clean up as
  your own user. Remove it manually first: `sudo rm -rf generated/<scenario-id>`

### Test silently runs against a stale image
- `build-test.py`'s rebuild stages always update the `:latest` tag, but
  `run_scenario.py` defaults to `--image-tag main`. Pass the matching tag
  explicitly after any raceboat SDK rebuild (see the image tag gotcha in
  [Quick Start](#quick-start)).

## Example: Racebird Plugin

Racebird is an adapter-based plugin (see `racebird/test/README.md`):

```
racebird/
├── test/
│   └── adapter.py                 # Generates race-cli params/channel/kit path for racebird
└── kit/
    └── artifacts/                 # Built plugin artifacts
```

```bash
cd raceboat/test/integration
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin
python3 run_scenario.py --scenario-id racebird-client-connect
```

## Architecture

This test infrastructure separates concerns:

- **Generic** (in raceboat): Test orchestration, TCP stubs, protocol
- **Plugin-specific** (in plugin repo): Docker compose config, plugin parameters, setup scripts

This allows:
- Plugin authors to focus only on their configuration
- Generic test improvements benefit all plugins
- Easy replication across different plugins
