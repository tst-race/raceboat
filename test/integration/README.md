# Raceboat Integration Test Framework

This directory contains generic infrastructure for testing raceboat plugins end-to-end using Docker Compose.

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

## Files

### Scripts
- **`run-integration-test.py`**: Generic test orchestrator (plugin-agnostic) - Python version
- **`tcp-stub-server.py`**: Server-side test stub that listens on port 7777
- **`tcp-stub-client.py`**: Client-side test stub that connects to port 9999
- **`example-docker-compose.yml`**: Template showing required Docker Compose structure

- **`README.md`**: This file

## Usage

### Basic Usage

```bash
./run-integration-test.py --compose-file /path/to/docker-compose.yml
# or
python3 run-integration-test.py --compose-file /path/to/docker-compose.yml
```

### With Options

```bash
./run-integration-test.py \
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

### Step 1: Create plugin-specific docker-compose.yml

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

### Step 2: Create setup script

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

### Step 3: Create wrapper integration test script

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

### Step 4: Run the test

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

## Example: Racebird Plugin

See the `racebird` plugin for a complete working example with both shell and Python versions:

```
racebird/
├── scripts/
│   ├── integration-test.sh       # Shell: Thin wrapper calling raceboat test
│   ├── integration-test.py       # Python: Thin wrapper calling raceboat test
│   ├── setup.sh                  # Shell: Plugin-specific setup
│   ├── setup.py                  # Python: Plugin-specific setup
│   └── docker-compose.yml        # Plugin-specific configuration
└── kit/
    └── artifacts/                # Built plugin artifacts
```

## Architecture

This test infrastructure separates concerns:

- **Generic** (in raceboat): Test orchestration, TCP stubs, protocol
- **Plugin-specific** (in plugin repo): Docker compose config, plugin parameters, setup scripts

This allows:
- Plugin authors to focus only on their configuration
- Generic test improvements benefit all plugins
- Easy replication across different plugins
