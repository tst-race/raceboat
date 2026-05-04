#!/usr/bin/env python3
"""
Generic TCP server stub for race-cli integration testing.
Listens on port 7777 for race-cli --server-connect to connect to.
Receives a message, sends a reply, and exits with a clear status.
Automatically handles both IPv4 and IPv6.
"""

import socket
import sys

PORT = 7777
EXPECTED = b"hello from client"
REPLY = b"hello from server\n"
TIMEOUT = 60

# Try IPv6 first (which usually supports IPv4 via dual-stack), then fall back to IPv4
for family, host in [(socket.AF_INET6, "::"), (socket.AF_INET, "0.0.0.0")]:
    try:
        s = socket.socket(family, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # For IPv6, try to enable dual-stack (IPv4 and IPv6)
        if family == socket.AF_INET6:
            try:
                s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            except (AttributeError, OSError):
                pass  # Not supported on this system
        s.bind((host, PORT))
        break  # Bind successful
    except (OSError, socket.error) as e:
        s.close()
        if family == socket.AF_INET:
            # Both families failed
            print("SERVER_FAIL:could not bind to port {}".format(PORT))
            sys.exit(3)
        continue

try:
    s.listen(1)
    s.settimeout(TIMEOUT)
    conn, addr = s.accept()
    with conn:
        conn.settimeout(TIMEOUT)
        data = b""
        while b"\n" not in data:
            chunk = conn.recv(1024)
            if not chunk:
                break
            data += chunk
        if EXPECTED in data:
            conn.sendall(REPLY)
            print("SERVER_OK:received=" + data.decode(errors="replace").strip())
            sys.exit(0)
        else:
            print("SERVER_FAIL:unexpected=" + repr(data))
            sys.exit(1)
except socket.timeout:
    print("SERVER_FAIL:timeout waiting for connection or data")
    sys.exit(2)
except Exception as e:
    print("SERVER_FAIL:" + str(e))
    sys.exit(3)
finally:
    s.close()
