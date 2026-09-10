# Build-Test Orchestrator Guide

## Overview

The `build-test.py` script provides fine-grained control over the rebuild-test workflow for raceboat plugins. It allows you to rebuild only what's necessary before running integration tests.

## Quick Start

```bash
# Navigate to the integration test directory
cd raceboat/test/integration

# Test without any rebuilds (fastest) - using relative path
python3 build-test.py --plugin-dir ../../racebird

# Rebuild plugin and test (common during plugin development)
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin

# Rebuild everything (after pulling changes)
python3 build-test.py --plugin-dir ../../racebird --rebuild-all

# Using absolute path
python3 build-test.py --plugin-dir /path/to/my-plugin --rebuild-plugin
```

## Build Pipeline

The complete build pipeline consists of these stages:

```
1. Raceboat Code         → Compile raceboat SDK in Docker
   ↓
2. Plugin Builder Image  → Docker image for compiling plugins
   ↓
3. Runtime Image         → Docker image with race-cli for tests
   ↓
4. Plugin Build          → Compile specific plugin + copy artifacts
   ↓
5. Integration Test      → Run docker-compose end-to-end test
```

## Rebuild Options

### High-Level Options

| Flag | Description | Rebuilds |
|------|-------------|----------|
| `--rebuild-all` | Rebuild everything | Stages 1-4 |
| `--rebuild-raceboat` | Rebuild all raceboat stages | Stages 1-3 |
| `--rebuild-plugin` | Rebuild only the plugin | Stage 4 |

### Granular Options

| Flag | Description | Rebuilds |
|------|-------------|----------|
| `--rebuild-raceboat-code` | Compile raceboat only | Stage 1 |
| `--rebuild-raceboat-images` | Docker images only | Stages 2-3 |
| `--rebuild-plugin-builder` | Plugin builder image | Stage 2 |
| `--rebuild-runtime-image` | Runtime image | Stage 3 |

### Control Options

| Flag | Description |
|------|-------------|
| `--skip-build` | Skip all builds, just run test |
| `--skip-test` | Build but don't run test |
| `--dry-run` | Show what would be done |
| `-v, --verbose` | Verbose output |
| `--wait-time <sec>` | Channel establishment wait time (default: 10) |

## Common Workflows

### Plugin Development

You're actively developing a plugin:

```bash
# Quick iteration - rebuild plugin only
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin

# Build plugin with verbose output for debugging
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin -v
```

### Raceboat SDK Development

You're modifying raceboat code:

```bash
# Rebuild just the raceboat code (fastest)
python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat-code

# Rebuild all raceboat stages (if changes affect images)
python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat
```

### After Pulling Changes

Someone updated the repository:

```bash
# Full rebuild to ensure everything is fresh
python3 build-test.py --plugin-dir ../../racebird --rebuild-all

# With verbose output to see what's happening
python3 build-test.py --plugin-dir ../../racebird --rebuild-all -v
```

### Quick Testing

Test with existing artifacts:

```bash
# No rebuilds - fastest option
python3 build-test.py --plugin-dir ../../racebird

# Explicitly skip builds
python3 build-test.py --plugin-dir ../../racebird --skip-build
```

### Build Without Testing

Build artifacts but don't run tests:

```bash
# Rebuild plugin but skip test
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin --skip-test

# Rebuild everything but skip test
python3 build-test.py --plugin-dir ../../racebird --rebuild-all --skip-test
```

### Planning / Dry-Run

See what would happen without executing:

```bash
# Preview the build plan
python3 build-test.py --plugin-dir ../../racebird --rebuild-all --dry-run

# Verbose dry-run shows detailed commands
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin -v --dry-run
```

## Supported Plugins

Any plugin following the standard structure can be used:
- Plugin directory must contain `build_artifacts_in_docker_image.sh`
- Plugin must have `test/setup.py` script
- Plugin must have `test/docker-compose.yml` configuration

Examples from this workspace:
```bash
# Racebird (obfs4 pluggable transport)
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin

# Bluesky transport
python3 build-test.py --plugin-dir ../../bluesky-transport --rebuild-plugin

# Mastodon transport
python3 build-test.py --plugin-dir ../../mastodon-transport --rebuild-plugin

# Your custom plugin (absolute path)
python3 build-test.py --plugin-dir /home/dev/my-custom-plugin --rebuild-all
```

## Understanding Dependencies

Build stages have these dependencies:

```
raceboat-code (Stage 1)
    ├── plugin-builder (Stage 2) - needs raceboat SDK
    └── runtime-image (Stage 3) - needs raceboat SDK

plugin-builder (Stage 2)
    └── plugin-build (Stage 4) - needs builder image

runtime-image (Stage 3)
    └── integration-test (Stage 5) - needs race-cli

plugin-build (Stage 4)
    └── integration-test (Stage 5) - needs plugin artifacts
```

**Important**: When you rebuild stage 1, stages 2 and 3 may need rebuilding too (depending on what changed).

## Examples by Scenario

### Scenario: Just changed plugin code

```bash
# Only rebuild the plugin
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin
```

### Scenario: Just changed raceboat SDK code

```bash
# Rebuild raceboat code only
python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat-code
```

### Scenario: Changed raceboat Dockerfile

```bash
# Rebuild the specific image
python3 build-test.py --plugin-dir ../../racebird --rebuild-runtime-image

# Or rebuild both images
python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat-images
```

### Scenario: First time setup / Major changes

```bash
# Rebuild everything from scratch
python3 build-test.py --plugin-dir ../../racebird --rebuild-all
```

### Scenario: Need longer channel establishment time

```bash
# Increase wait time to 20 seconds
python3 build-test.py --plugin-dir ../../racebird --wait-time 20
```

### Scenario: Want to see what's happening

```bash
# Verbose mode shows commands and details
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin -v
```

## Troubleshooting

### Problem: "Image not found" error during test

```bash
# Build the missing runtime image
python3 build-test.py --plugin-dir ../../racebird --rebuild-runtime-image
```

### Problem: Plugin build fails

```bash
# Rebuild plugin builder image first, then plugin
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin-builder --rebuild-plugin
```

### Problem: Unsure what to rebuild

```bash
# Use dry-run to see the plan
python3 build-test.py --plugin-dir ../../racebird --rebuild-all --dry-run

# Then execute specific stages you need
python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat-code
```

### Problem: Want to see exact commands

```bash
# Use verbose flag
python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin -v
```

### Problem: Builds seem stale or broken

```bash
# Clean Docker system (WARNING: removes unused containers/images)
docker system prune -a

# Then rebuild everything
python3 build-test.py --plugin-dir ../../racebird --rebuild-all
```

## Tips

1. **Start minimal**: Use `--skip-build` or no flags for fastest iteration
2. **Build incrementally**: Only rebuild what you changed
3. **Use dry-run**: Preview before long rebuilds (`--dry-run`)
4. **Check existing images**: Run `docker images | grep raceboat`
5. **Verbose debugging**: Add `-v` flag when troubleshooting
6. **Test integration**: The test uses docker-compose under the hood

## Related Scripts

- **`run-integration-test.py`** - Generic test orchestrator (called by build-test.py)
- **`setup.py`** - Per-plugin script to copy artifacts (called during plugin build)
- **`tcp-stub-client.py` / `tcp-stub-server.py`** - Test stubs for integration test
- **`docker-compose.yml`** - Per-plugin compose file for test containers

## Full Help

```bash
python3 build-test.py --help
```

## Advanced: CI/CD Integration

### Simple CI Pipeline

```bash
#!/bin/bash
# ci-test.sh - Run full integration test

set -e

cd raceboat/test/integration

# Rebuild everything for reproducibility
python3 build-test.py --plugin-dir ../../racebird --rebuild-all --verbose
```

### Multi-Plugin CI Pipeline

```bash
#!/bin/bash
# ci-test-all.sh - Test all plugins

set -e

cd raceboat/test/integration

# Build raceboat once
python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat --skip-test

# Test each plugin
for plugin_dir in ../../racebird ../../bluesky-transport ../../mastodon-transport; do
    plugin_name=$(basename $plugin_dir)
    echo "Testing $plugin_name..."
    python3 build-test.py --plugin-dir $plugin_dir --rebuild-plugin || exit 1
done

echo "All plugins passed!"
```

### Incremental CI (checks what changed)

```bash
#!/bin/bash
# ci-incremental.sh - Smart rebuild based on changes

set -e

cd raceboat/test/integration

# Check what changed (simplified example)
if git diff --name-only HEAD~1 | grep -q "^raceboat/source"; then
    echo "Raceboat code changed, full rebuild"
    python3 build-test.py --plugin-dir ../../racebird --rebuild-all
elif git diff --name-only HEAD~1 | grep -q "^racebird/source"; then
    echo "Racebird changed, rebuild plugin only"
    python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin
else
    echo "No relevant changes, test only"
    python3 build-test.py --plugin-dir ../../racebird --skip-build
fi
```
