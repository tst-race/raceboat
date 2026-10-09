#!/usr/bin/env python3
"""
run-send-recv-test.py
Integration test runner for race-cli's "send" and "send-recv" scenario modes
(see generate_scenario.py's MODE_ROLE_FLAGS/STDIN_MESSAGE_ROLES). Unlike
run-integration-test.py's --client-connect/--server-connect modes, these
modes don't proxy a persistent local socket through a stub client/server -
each race-cli process itself reads one message from stdin, sends/receives it,
and (for "send-recv") prints the reply, then exits on its own. Validation is
therefore just: bring the listener up first, bring the connector up once the
listener should be ready, wait for the connector to exit, and grep each
container's own stdout for the expected message(s).
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
BLUE = '\033[0;34m'
NC = '\033[0m'


def colored(text, color):
    return f"{color}{text}{NC}"


def docker_compose_down(compose_file):
    print(f"\n{colored('Stopping containers...', YELLOW)}")
    subprocess.run(
        ['docker', 'compose', '-f', compose_file, 'down'],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def fail(message, compose_file, listener_container, connector_container):
    print(f"\n{colored('=' * 39, BLUE)}")
    print(colored('INTEGRATION TEST FAILED ✗', RED))
    print(colored('=' * 39, BLUE))
    print(f"\n{colored('Check container logs:', YELLOW)}")
    print(f"  docker logs {listener_container}")
    print(f"  docker logs {connector_container}")
    if message:
        print(f"\n{colored(message, RED)}")
    docker_compose_down(compose_file)
    sys.exit(1)


def container_logs(container: str) -> str:
    result = subprocess.run(
        ['docker', 'logs', container], capture_output=True, text=True,
    )
    return (result.stdout or "") + (result.stderr or "")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compose-file', required=True)
    parser.add_argument('--wait-time', type=int, default=10,
                         help='Seconds to let the listener start before starting the connector')
    parser.add_argument('--exit-timeout', type=int, default=30,
                         help='Seconds to wait for the connector (and, for send-recv, the '
                              'listener) to exit on its own after sending')
    parser.add_argument('--name', default='Send/Recv Integration Test')
    parser.add_argument('--listener-container', default='listener')
    parser.add_argument('--connector-container', default='dialer')
    parser.add_argument('--mode', required=True, choices=['send', 'send-recv'])
    parser.add_argument('--send-message', required=True,
                         help='Message the connector sends (expected in the listener log)')
    parser.add_argument('--reply-message', default=None,
                         help='Reply the listener sends back for send-recv mode (expected in '
                              'the connector log)')
    args = parser.parse_args()

    compose_file = args.compose_file
    listener_container = args.listener_container
    connector_container = args.connector_container

    if not Path(compose_file).is_file():
        print(colored(f"Error: Compose file not found: {compose_file}", RED))
        return 1

    print(colored('=' * 39, BLUE))
    print(colored(args.name, BLUE))
    print(colored('=' * 39, BLUE))

    print(f"\n{colored('[1/3] Starting listener...', YELLOW)}")
    result = subprocess.run(
        ['docker', 'compose', '-f', compose_file, 'up', '-d', listener_container],
        check=False,
    )
    if result.returncode != 0:
        fail("Failed to start listener", compose_file, listener_container, connector_container)
    print(f"Waiting {args.wait_time}s for listener to be ready to receive...")
    time.sleep(args.wait_time)

    print(f"\n{colored('[2/3] Starting connector...', YELLOW)}")
    result = subprocess.run(
        ['docker', 'compose', '-f', compose_file, 'up', '-d', connector_container],
        check=False,
    )
    if result.returncode != 0:
        fail("Failed to start connector", compose_file, listener_container, connector_container)

    print(f"Waiting up to {args.exit_timeout}s for connector to finish sending...")
    wait_result = subprocess.run(
        ['docker', 'wait', connector_container],
        capture_output=True, text=True, timeout=args.exit_timeout + 10,
    )
    if wait_result.returncode != 0:
        fail(f"connector container did not exit cleanly: {wait_result.stderr.strip()}",
             compose_file, listener_container, connector_container)

    print(f"\n{colored('[3/3] Evaluating results...', YELLOW)}")
    listener_log = container_logs(listener_container)
    connector_log = container_logs(connector_container)

    listener_ok = args.send_message in listener_log
    print(f"  listener received expected message: {listener_ok}")

    connector_ok = True
    if args.mode == 'send-recv':
        if args.reply_message is None:
            fail("--reply-message is required for send-recv mode",
                 compose_file, listener_container, connector_container)
        connector_ok = args.reply_message in connector_log
        print(f"  connector received expected reply: {connector_ok}")

    docker_compose_down(compose_file)

    if listener_ok and connector_ok:
        print(f"\n{colored('=' * 39, BLUE)}")
        print(colored('INTEGRATION TEST PASSED ✓', GREEN))
        print(colored('=' * 39, BLUE))
        return 0

    print(f"\n{colored('Listener log:', YELLOW)}\n{listener_log}")
    print(f"\n{colored('Connector log:', YELLOW)}\n{connector_log}")
    print(f"\n{colored('=' * 39, BLUE)}")
    print(colored('INTEGRATION TEST FAILED ✗', RED))
    print(colored('=' * 39, BLUE))
    return 1


if __name__ == '__main__':
    sys.exit(main())
