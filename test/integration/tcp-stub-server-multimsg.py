#!/usr/bin/env python3
"""
Multi-round-trip TCP server stub for race-cli integration testing.

Like tcp-stub-server.py, but exchanges NUM_MESSAGES sequential messages per
client connection, verifying each message's index arrives in strict order
and that the client id embedded in a connection's messages never changes
mid-stream - a proxy for detecting cross-client corruption/interleaving when
multiple clients share one link address (only PackageIds/streamTag at the
SDK layer disambiguate them; from a TCP stub's point of view, corruption
shows up as out-of-order indices or a different client's data appearing).

Usage: tcp-stub-server-multimsg.py [num_clients] [num_messages]
"""

import socket
import sys
import threading

PORT = 7777
# Generous per-message timeout: polling-based channels (e.g. decomposed-
# exemplars' whiteboard) can need up to ~2 poll cycles per round trip, and
# occasional multi-cycle delays under multi-client contention over many
# sequential messages have been observed to exceed 60-120s.
TIMEOUT = 300
NUM_CLIENTS = int(sys.argv[1]) if len(sys.argv) > 1 else 1
NUM_MESSAGES = int(sys.argv[2]) if len(sys.argv) > 2 else 1

connections_lock = threading.Lock()
successful_connections = []
failed_connections = []


def recv_line(conn, buffer):
    while b"\n" not in buffer[0]:
        chunk = conn.recv(1024)
        if not chunk:
            return None
        buffer[0] += chunk
    line, _, rest = buffer[0].partition(b"\n")
    buffer[0] = rest
    return line


def handle_client(conn, client_num):
    buffer = [b""]
    client_id = None
    try:
        conn.settimeout(TIMEOUT)
        for i in range(1, NUM_MESSAGES + 1):
            line = recv_line(conn, buffer)
            if line is None:
                with connections_lock:
                    failed_connections.append(f"client{client_num}:closed early at msg{i}")
                return
            parts = line.decode(errors="replace").split()
            if len(parts) < 4 or parts[0:2] != ["hello", "from"] or parts[3] != f"msg{i}":
                with connections_lock:
                    failed_connections.append(f"client{client_num}:unexpected msg{i}={line!r}")
                return
            this_id = parts[2]
            if client_id is None:
                client_id = this_id
            elif this_id != client_id:
                with connections_lock:
                    failed_connections.append(
                        f"client{client_num}:id changed mid-connection ({client_id}->{this_id}) at msg{i}"
                    )
                return
            conn.sendall(f"hello from server ack msg{i} for {this_id}\n".encode())
        with connections_lock:
            successful_connections.append(f"client{client_num}:{client_id}:{NUM_MESSAGES} messages")
    except socket.timeout:
        with connections_lock:
            failed_connections.append(f"client{client_num}:timeout")
    except Exception as e:
        with connections_lock:
            failed_connections.append(f"client{client_num}:error={str(e)}")
    finally:
        conn.close()


s = None
for family, host in [(socket.AF_INET6, "::"), (socket.AF_INET, "0.0.0.0")]:
    try:
        s = socket.socket(family, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if family == socket.AF_INET6:
            try:
                s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            except (AttributeError, OSError):
                pass
        s.bind((host, PORT))
        break
    except (OSError, socket.error):
        s.close()
        s = None
        if family == socket.AF_INET:
            print("SERVER_FAIL:could not bind to port {}".format(PORT))
            sys.exit(3)
        continue

if s is None:
    print("SERVER_FAIL:could not create socket")
    sys.exit(3)

try:
    s.listen(NUM_CLIENTS)
    s.settimeout(TIMEOUT)

    threads = []
    for i in range(NUM_CLIENTS):
        conn, addr = s.accept()
        thread = threading.Thread(target=handle_client, args=(conn, i + 1))
        thread.start()
        threads.append(thread)

    for thread in threads:
        thread.join()

    if len(successful_connections) == NUM_CLIENTS and not failed_connections:
        print("SERVER_OK:received={} clients:{}".format(NUM_CLIENTS, ",".join(successful_connections)))
        sys.exit(0)
    else:
        print("SERVER_FAIL:expected={} success={} failed={}:{}".format(
            NUM_CLIENTS, len(successful_connections), len(failed_connections),
            ",".join(failed_connections) if failed_connections else "none"
        ))
        sys.exit(1)
except socket.timeout:
    print("SERVER_FAIL:timeout waiting for {} connections (got {})".format(
        NUM_CLIENTS, len(successful_connections) + len(failed_connections)
    ))
    sys.exit(2)
except Exception as e:
    print("SERVER_FAIL:" + str(e))
    sys.exit(3)
finally:
    s.close()
