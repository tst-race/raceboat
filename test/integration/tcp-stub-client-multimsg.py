#!/usr/bin/env python3
"""
Multi-round-trip TCP client stub for race-cli integration testing.

Like tcp-stub-client.py, but keeps ONE persistent connection open and
exchanges NUM_MESSAGES sequential messages before exiting (which closes the
local socket). Each message embeds the client id and its own index so the
server can detect corruption/interleaving (e.g. two clients sharing one
link address getting their streams mixed up) instead of just checking for
"any" reply.

Usage: tcp-stub-client-multimsg.py <client_id> [num_messages]
"""

import socket
import sys
import time

PORT = 9999
CLIENT_ID = sys.argv[1] if len(sys.argv) > 1 else "client"
NUM_MESSAGES = int(sys.argv[2]) if len(sys.argv) > 2 else 1
# Generous per-message timeout: polling-based channels (e.g. decomposed-
# exemplars' whiteboard) can need up to ~2 poll cycles per round trip, and
# occasional multi-cycle delays under multi-client contention over many
# sequential messages have been observed to exceed 60-120s.
TIMEOUT = 300
MAX_RETRIES = 60
RETRY_DELAY = 1


def try_connect(family, host):
    s = socket.socket(family, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    s.connect((host, PORT))
    return s


def connect_with_retries():
    for attempt in range(MAX_RETRIES):
        for family, host in [(socket.AF_INET, "127.0.0.1"), (socket.AF_INET6, "::1")]:
            try:
                return try_connect(family, host)
            except (OSError, socket.error):
                continue
        if attempt < MAX_RETRIES - 1:
            time.sleep(RETRY_DELAY)
    return None


def recv_line(sock):
    data = b""
    while b"\n" not in data:
        chunk = sock.recv(1024)
        if not chunk:
            break
        data += chunk
    return data


sock = connect_with_retries()
if sock is None:
    print("CLIENT_FAIL:could not connect after {} attempts".format(MAX_RETRIES))
    sys.exit(2)

try:
    for i in range(1, NUM_MESSAGES + 1):
        sock.sendall(f"hello from {CLIENT_ID} msg{i}\n".encode())
        data = recv_line(sock)
        expected = f"ack msg{i} for {CLIENT_ID}".encode()
        if expected not in data:
            print("CLIENT_FAIL:msg{}:unexpected={!r}".format(i, data))
            sys.exit(1)
    print("CLIENT_OK:completed {} messages".format(NUM_MESSAGES))
    sys.exit(0)
except socket.timeout:
    print("CLIENT_FAIL:timeout")
    sys.exit(3)
except Exception as e:
    print("CLIENT_FAIL:" + str(e))
    sys.exit(4)
finally:
    sock.close()
