#!/usr/bin/env python3
"""
Generic TCP client stub for race-cli integration testing.
Connects to port 9999 where race-cli --client-connect listens.
Sends a message, receives a reply, and exits with a clear status.
Retries to allow time for race-cli to establish the RACE channel.
Automatically handles both IPv4 and IPv6.
"""

import socket
import sys
import time

PORT = 9999
MESSAGE = b"hello from client\n"
EXPECTED = b"hello from server"
TIMEOUT = 30
MAX_RETRIES = 60
RETRY_DELAY = 1

def try_connect(family, host):
    """Try to connect using the specified address family."""
    s = socket.socket(family, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    s.connect((host, PORT))
    return s

for attempt in range(MAX_RETRIES):
    sock = None
    try:
        # Try IPv4 first, then IPv6
        for family, host in [(socket.AF_INET, "127.0.0.1"), (socket.AF_INET6, "::1")]:
            try:
                sock = try_connect(family, host)
                break  # Connection successful
            except (OSError, socket.error) as e:
                # If address family not supported or connection refused, try next family
                if family == socket.AF_INET6 and sock is None:
                    # Both failed, raise the last error
                    raise
                continue
        
        if sock is None:
            raise ConnectionRefusedError("Could not connect with IPv4 or IPv6")
            
        sock.sendall(MESSAGE)
        data = b""
        while b"\n" not in data:
            chunk = sock.recv(1024)
            if not chunk:
                break
            data += chunk
        sock.close()
        
        if EXPECTED in data:
            print("CLIENT_OK:received=" + data.decode(errors="replace").strip())
            sys.exit(0)
        else:
            print("CLIENT_FAIL:unexpected=" + repr(data))
            sys.exit(1)
    except (ConnectionRefusedError, OSError):
        if sock:
            sock.close()
        if attempt < MAX_RETRIES - 1:
            time.sleep(RETRY_DELAY)
        else:
            print("CLIENT_FAIL:could not connect after {} attempts".format(MAX_RETRIES))
            sys.exit(2)
    except socket.timeout:
        if sock:
            sock.close()
        print("CLIENT_FAIL:timeout")
        sys.exit(3)
    except Exception as e:
        if sock:
            sock.close()
        print("CLIENT_FAIL:" + str(e))
        sys.exit(4)
