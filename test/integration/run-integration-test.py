#!/usr/bin/env python3
"""
run-integration-test.py
Generic integration test runner for race-cli plugins.
Tests bidirectional message delivery through race-cli --server-connect and --client-connect modes.
"""

import argparse
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

# ANSI color codes
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
BLUE = '\033[0;34m'
NC = '\033[0m'  # No Color


def colored(text, color):
    """Print colored text."""
    return f"{color}{text}{NC}"


def run_command(cmd, capture_output=False, check=True, text=True, shell=False):
    """Run a command and optionally capture output."""
    try:
        if capture_output:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=text,
                check=check,
                shell=shell
            )
            return result
        else:
            result = subprocess.run(cmd, check=check, shell=shell)
            return result
    except subprocess.CalledProcessError as e:
        return e


def docker_compose_down(compose_file):
    """Stop docker compose containers."""
    print(f"\n{colored('Stopping containers...', YELLOW)}")
    subprocess.run(
        ['docker', 'compose', '-f', compose_file, 'down'],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )


def fail(message, server_container, client_container):
    """Print failure message and exit."""
    print(f"\n{colored('=' * 39, BLUE)}")
    print(colored('INTEGRATION TEST FAILED ✗', RED))
    print(colored('=' * 39, BLUE))
    print(f"\n{colored('Check container logs:', YELLOW)}")
    print(f"  docker logs {server_container}")
    print(f"  docker logs {client_container}")
    if message:
        print(f"\n{colored(message, RED)}")
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Generic integration test runner for race-cli plugins.'
    )
    parser.add_argument(
        '--compose-file',
        required=True,
        help='Path to docker-compose.yml (required)'
    )
    parser.add_argument(
        '--wait-time',
        type=int,
        default=10,
        help='Time to wait for RACE channel (default: 10)'
    )
    parser.add_argument(
        '--name',
        default='Race-CLI Integration Test',
        help='Test name for output (default: Race-CLI Integration Test)'
    )
    parser.add_argument(
        '--server-container',
        default='rbserver',
        help='Server container name (default: rbserver)'
    )
    parser.add_argument(
        '--client-container',
        default='rbclient',
        help='Client container name (default: rbclient)'
    )

    args = parser.parse_args()

    # Validate compose file
    if not os.path.isfile(args.compose_file):
        print(colored(f"Error: Compose file not found: {args.compose_file}", RED))
        sys.exit(1)

    script_dir = Path(__file__).parent.resolve()
    compose_file = args.compose_file
    wait_time = args.wait_time
    test_name = args.name
    server_container = args.server_container
    client_container = args.client_container

    # Create temporary files for output
    server_out_fd, server_out_path = tempfile.mkstemp()
    client_out_fd, client_out_path = tempfile.mkstemp()
    
    try:
        # Print header
        print(colored('=' * 39, BLUE))
        print(colored(test_name, BLUE))
        print(colored('=' * 39, BLUE))

        # Step 1: Start containers
        print(f"\n{colored('[1/4] Starting containers...', YELLOW)}")
        result = run_command(
            ['docker', 'compose', '-f', compose_file, 'up', '-d'],
            check=False
        )
        if isinstance(result, subprocess.CalledProcessError) or result.returncode != 0:
            docker_compose_down(compose_file)
            fail("Failed to start containers", server_container, client_container)
        print(colored('✓ Containers started', GREEN))

        # Wait for containers to be ready
        print("Waiting for containers to become ready...")
        ready = False
        for i in range(30):
            server_ready = subprocess.run(
                ['docker', 'exec', server_container, 'true'],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            ).returncode == 0
            client_ready = subprocess.run(
                ['docker', 'exec', client_container, 'true'],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            ).returncode == 0
            
            if server_ready and client_ready:
                ready = True
                break
            
            if i == 29:
                docker_compose_down(compose_file)
                fail("Containers did not become ready in time", server_container, client_container)
            
            time.sleep(1)

        # Step 2: Wait for RACE channel to establish
        print(f"\n{colored(f'[2/4] Waiting {wait_time}s for RACE channel to establish...', YELLOW)}")
        time.sleep(wait_time)

        # Step 3: Run test stubs
        print(f"\n{colored('[3/4] Running integration test stubs...', YELLOW)}")

        # Copy stubs into containers
        server_stub = script_dir / 'tcp-stub-server.py'
        client_stub = script_dir / 'tcp-stub-client.py'
        
        subprocess.run(
            ['docker', 'cp', str(server_stub), f'{server_container}:/tmp/stub-server.py'],
            check=True
        )
        subprocess.run(
            ['docker', 'cp', str(client_stub), f'{client_container}:/tmp/stub-client.py'],
            check=True
        )

        # Start server stub in background
        print("Starting server stub...")
        subprocess.run(
            ['docker', 'exec', '-d', server_container, 'bash', '-c',
             'python3 /tmp/stub-server.py >/tmp/stub-server.out 2>&1'],
            check=True
        )
        time.sleep(2)

        # Run client stub
        print("Starting client stub...")
        result = subprocess.run(
            ['docker', 'exec', client_container, 'python3', '/tmp/stub-client.py'],
            capture_output=True,
            text=True
        )
        client_exit = result.returncode
        print(f"Client stub exited with code: {client_exit}")

        # Write client output to temp file
        with os.fdopen(client_out_fd, 'w') as f:
            f.write(result.stdout)
            if result.stderr:
                f.write(result.stderr)
        client_out_fd = None  # Mark as closed

        # Get server stub output from container
        result = subprocess.run(
            ['docker', 'exec', server_container, 'cat', '/tmp/stub-server.out'],
            capture_output=True,
            text=True
        )
        server_output = result.stdout if result.returncode == 0 else "SERVER_FAIL:no output"

        # Write server output to temp file
        with os.fdopen(server_out_fd, 'w') as f:
            f.write(server_output)
        server_out_fd = None  # Mark as closed

        # Step 4: Evaluate results
        print(f"\n{colored('[4/4] Evaluating results...', YELLOW)}")

        # Read results
        with open(server_out_path, 'r') as f:
            server_result = f.read().strip() or "SERVER_FAIL:could not read output"
        with open(client_out_path, 'r') as f:
            client_result = f.read().strip() or "CLIENT_FAIL:could not read output"

        print(f"  Server: {server_result}")
        print(f"  Client: {client_result}")

        # Evaluate
        if server_result.startswith("SERVER_OK:") and client_result.startswith("CLIENT_OK:"):
            print(f"\n{colored('=' * 39, BLUE)}")
            print(colored('INTEGRATION TEST PASSED ✓', GREEN))
            print(colored('=' * 39, BLUE))
            print(colored('  Client→Server: message received', GREEN))
            print(colored('  Server→Client: reply received', GREEN))
            print()
            return 0
        else:
            print()
            if not server_result.startswith("SERVER_OK:"):
                print(colored(f"  Server failure: {server_result}", RED))
            if not client_result.startswith("CLIENT_OK:"):
                print(colored(f"  Client failure: {client_result}", RED))
            print()
            docker_compose_down(compose_file)
            fail("", server_container, client_container)

    finally:
        # Cleanup temp files
        if server_out_fd is not None:
            os.close(server_out_fd)
        if client_out_fd is not None:
            os.close(client_out_fd)
        
        try:
            os.unlink(server_out_path)
        except:
            pass
        try:
            os.unlink(client_out_path)
        except:
            pass
        
        # Cleanup containers
        docker_compose_down(compose_file)


if __name__ == '__main__':
    sys.exit(main())
