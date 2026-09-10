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
    parser.add_argument(
        '--additional-clients',
        nargs='*',
        default=[],
        help='Additional client container names for multi-client testing (e.g., rbclient2 rbclient3)'
    )
    parser.add_argument(
        '--clear-logs',
        action='store_true',
        help='Clear log directories before running test'
    )
    parser.add_argument(
        '--log-dirs',
        nargs='*',
        default=[],
        help='Log directories to clear (relative to compose file directory)'
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
    all_clients = [client_container] + args.additional_clients
    num_clients = len(all_clients)

    # Create temporary files for output
    server_out_fd, server_out_path = tempfile.mkstemp()
    client_out_fds = []
    client_out_paths = []
    for i in range(num_clients):
        fd, path = tempfile.mkstemp()
        client_out_fds.append(fd)
        client_out_paths.append(path)
    
    try:
        # Print header
        print(colored('=' * 39, BLUE))
        print(colored(test_name, BLUE))
        print(colored('=' * 39, BLUE))

        # Step 0: Clear logs if requested
        if args.clear_logs:
            print(f"\n{colored('[0/4] Clearing log directories...', YELLOW)}")
            compose_dir = Path(compose_file).parent
            
            # Use specified log dirs, or auto-detect directories ending with '-logs'
            log_dirs = args.log_dirs if args.log_dirs else [
                d.name for d in compose_dir.iterdir() 
                if d.is_dir() and d.name.endswith('-logs')
            ]
            
            if not log_dirs:
                print(f"  No log directories found")
            else:
                for log_dir in log_dirs:
                    log_path = compose_dir / log_dir
                    if log_path.exists() and log_path.is_dir():
                        log_files = list(log_path.glob('*.log'))
                        if log_files:
                            for log_file in log_files:
                                try:
                                    log_file.unlink()
                                    print(f"  Cleared: {log_dir}/{log_file.name}")
                                except Exception as e:
                                    print(f"  Warning: Could not clear {log_dir}/{log_file.name}: {e}")
                        else:
                            print(f"  No logs in {log_dir}")
                    else:
                        print(f"  Skipping {log_dir} (not found)")
            print(colored('✓ Logs cleared', GREEN))

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
            
            clients_ready = all(
                subprocess.run(
                    ['docker', 'exec', client, 'true'],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                ).returncode == 0
                for client in all_clients
            )
            
            if server_ready and clients_ready:
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
        if num_clients > 1:
            print(f"Testing with {num_clients} simultaneous clients...")

        # Copy stubs into containers
        server_stub = script_dir / 'tcp-stub-server.py'
        client_stub = script_dir / 'tcp-stub-client.py'
        
        subprocess.run(
            ['docker', 'cp', str(server_stub), f'{server_container}:/tmp/stub-server.py'],
            check=True
        )
        
        for client in all_clients:
            subprocess.run(
                ['docker', 'cp', str(client_stub), f'{client}:/tmp/stub-client.py'],
                check=True
            )

        # Start server stub in background (with number of expected clients)
        print(f"Starting server stub (expecting {num_clients} clients)...")
        subprocess.run(
            ['docker', 'exec', '-d', server_container, 'bash', '-c',
             f'python3 /tmp/stub-server.py {num_clients} >/tmp/stub-server.out 2>&1'],
            check=True
        )
        time.sleep(2)

        # Run client stubs on all clients
        print(f"Starting {num_clients} client stub(s)...")
        client_results = []
        for idx, client in enumerate(all_clients):
            result = subprocess.run(
                ['docker', 'exec', client, 'python3', '/tmp/stub-client.py', client],
                capture_output=True,
                text=True
            )
            client_results.append((client, result))
            print(f"  {client} exited with code: {result.returncode}")

        # Write client outputs to temp files
        for idx, (client, result) in enumerate(client_results):
            with os.fdopen(client_out_fds[idx], 'w') as f:
                f.write(result.stdout)
                if result.stderr:
                    f.write(result.stderr)
        client_out_fds = [None] * num_clients  # Mark all as closed

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
        
        client_results_data = []
        for idx, client in enumerate(all_clients):
            with open(client_out_paths[idx], 'r') as f:
                client_result = f.read().strip() or f"CLIENT_FAIL:could not read output from {client}"
                client_results_data.append((client, client_result))

        print(f"  Server: {server_result}")
        for client, result in client_results_data:
            print(f"  {client}: {result}")

        # Evaluate
        server_ok = server_result.startswith("SERVER_OK:")
        all_clients_ok = all(result.startswith("CLIENT_OK:") for _, result in client_results_data)
        
        if server_ok and all_clients_ok:
            print(f"\n{colored('=' * 39, BLUE)}")
            print(colored('INTEGRATION TEST PASSED ✓', GREEN))
            print(colored('=' * 39, BLUE))
            for client, _ in client_results_data:
                print(colored(f'  {client}→Server: message received', GREEN))
                print(colored(f'  Server→{client}: reply received', GREEN))
            print()
            return 0
        else:
            print()
            if not server_ok:
                print(colored(f"  Server failure: {server_result}", RED))
            for client, result in client_results_data:
                if not result.startswith("CLIENT_OK:"):
                    print(colored(f"  {client} failure: {result}", RED))
            print()
            docker_compose_down(compose_file)
            fail("", server_container, client_container)

    finally:
        # Cleanup temp files
        if server_out_fd is not None:
            os.close(server_out_fd)
        for fd in client_out_fds:
            if fd is not None:
                os.close(fd)
        
        try:
            os.unlink(server_out_path)
        except:
            pass
        for path in client_out_paths:
            try:
                os.unlink(path)
            except:
                pass
        
        # Cleanup containers
        docker_compose_down(compose_file)


if __name__ == '__main__':
    sys.exit(main())
