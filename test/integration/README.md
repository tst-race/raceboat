# Raceboat Integration Test Framework

This directory contains generic infrastructure for testing raceboat plugins end-to-end using Docker Compose.

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
topology (node roles, IPs, and which plugin fills each "slot"). It's rendered
into a docker-compose file by `generate_scenario.py` (which calls each
participating plugin's `test/adapter.py`) and executed by `run_scenario.py`:

```bash
python3 run_scenario.py --scenario-id <scenario-id> --image-tag <tag>
```

- `--scenario-id` (required): name of a file under `scenarios/` (without `.json`)
- `--image-tag` (default: `main`): the `raceboat-runtime` tag to test against
- `--wait-time` (optional): overrides the scenario's own `wait_time` (RACE channel establishment delay)

Currently available scenarios:
- `racebird-client-connect`, `decomposed-client-connect`,
  `decomposed-client-connect-multi-client` - single-plugin `client-connect`/`server-connect` mode
- `bootstrap-decomposed-racebird`, `bootstrap-racebird-multi-client`,
  `bootstrap-racebird-racebird-multi-client`, `bootstrap-racebird-decomposed-multi-client`,
  `bootstrap-decomposed-decomposed-multi-client` - `bootstrap-connect` mode (one listener plus one
  or more connector nodes, each with an "initial" and "final" plugin slot)

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

## Files

### Scripts
- **`build-test.py`**: Rebuild-and-test orchestrator (raceboat SDK + plugin builds, then runs a scenario or legacy compose test) - see [BUILD_TEST_GUIDE.md](BUILD_TEST_GUIDE.md)
- **`run_scenario.py`**: Generates a scenario's docker-compose.yml and runs it through `run-integration-test.py`
- **`generate_scenario.py`**: Renders a `scenarios/*.json` topology into a docker-compose file by calling each plugin's `test/adapter.py`
- **`link_topology.py`**: Parses debug logs to verify bootstrap-connect final-link creator/loader counts
- **`run-integration-test.py`**: Generic test orchestrator (plugin-agnostic) that drives an already-generated docker-compose.yml
- **`tcp-stub-server.py`**: Server-side test stub that listens on port 7777
- **`tcp-stub-client.py`**: Client-side test stub that connects to port 9999
- **`adapter_types.py`**: `NodeRequest`/`NodeContribution` dataclasses - the contract every `test/adapter.py` implements
- **`plugin_registry.json`**: Maps plugin name -> plugin directory, so `generate_scenario.py` can find each plugin's `test/adapter.py`
- **`example-docker-compose.yml`**: Template for legacy (non-adapter) plugin compose files

### Data
- **`scenarios/*.json`**: Declarative topology definitions (nodes, roles, plugin slots) consumed by `run_scenario.py`
- **`generated/<scenario-id>/`**: Per-run output (compose file, merged kits, logs) - safe to delete between runs

- **`README.md`**: This file

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
- `--server-container <name>`: Server container name (default: rbserver)
- `--client-container <name>`: Client container name (default: rbclient)
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
    --client-container rbclient \
    --additional-clients rbclient2 rbclient3
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
- Default: `rbserver` and `rbclient`
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

def generate_node_contribution(request: NodeRequest) -> NodeContribution:
    return NodeContribution(
        params={"YourPlugin.node-id": "..."},
        channel_name="yourChannelGid",
        kit_dir=script_dir / ".." / "kit" / "artifacts" / "linux-x86_64-server" / "PluginYourPlugin",
        address_output={"...": "..."} if request.role == "listener" else None,
        needs_peer_address=request.role == "connector",
    )
```

`generate_node_contribution` is called once per active "slot"
(`channel`/`initial`/`final`) for every node in the scenario; see
`adapter_types.py` for the full field-by-field contract.

#### Step 2: Register the plugin

Add an entry to `plugin_registry.json`:

```json
{
  "your-plugin": "../../../your-plugin"
}
```

#### Step 3: Add a scenario

Add `scenarios/your-plugin-client-connect.json` describing a listener and one
or more connector nodes with `"slots": {"channel": "your-plugin"}` (or
`"initial"`/`"final"` for `bootstrap-connect` scenarios).

#### Step 4: Run it

```bash
python3 build-test.py --plugin-dir ../../your-plugin --rebuild-plugin
# or, once built:
python3 run_scenario.py --scenario-id your-plugin-client-connect
```

### Legacy: hand-written docker-compose.yml

Plugins that haven't migrated to `test/adapter.py` still work via a
plugin-owned `docker-compose.yml` + `setup.py`, run directly through
`run-integration-test.py`.

#### Step 1: Create plugin-specific docker-compose.yml

Create a `docker-compose.yml` with your plugin's specific parameters:

```yaml
services:
  rbserver:
    image: ghcr.io/tst-race/raceboat/raceboat-runtime:latest
    command: >
      race-cli -m --server-connect --debug
      --logto /tmp/raceboat_server.log
      --param YourPlugin.param1="value1"
      --param YourPlugin.param2="value2"
  
  rbclient:
    image: ghcr.io/tst-race/raceboat/raceboat-runtime:latest
    command: >
      race-cli -m --client-connect --debug
      --logto /tmp/raceboat_client.log
      --send-address="YOUR_LINK_ADDRESS"
```

This compose file will be invoked by the integration-test script to run the test. The rbclient and rbserver containers must exist and use the specified images (or images built on top of them), and the raceboat command must be invoked. However, the arguments to the race-cli command (e.g. the send address, the params, etc.) will be customized to the plugin being tested. Additionally, for channels reliant on additional self-hosted services (e.g. if testing a "localized" version of an email channel that relies on the existence of an email server) those can be added to the docker-compose file to support a fully automated test.

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
- Check raceboat logs: `docker logs rbserver` and `docker logs rbclient`
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
