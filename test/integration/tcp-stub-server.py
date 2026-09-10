#!/usr/bin/env python3
"""
Generic TCP server stub for race-cli integration testing.
Listens on port 7777 for race-cli --server-connect to connect to.
Receives a message, sends a reply, and exits with a clear status.
Automatically handles both IPv4 and IPv6.
Supports multiple simultaneous client connections.
"""

import socket
import sys
import threading
import time

PORT = 7777
EXPECTED = b"hello from"  # Accept any client ID
REPLY = b"hello from server\n"
TIMEOUT = 60
NUM_CLIENTS = int(sys.argv[1]) if len(sys.argv) > 1 else 1

# Shared state for tracking connections
connections_lock = threading.Lock()
successful_connections = []
failed_connections = []

def handle_client(conn, addr, client_num):
    """Handle a single client connection."""
    try:
        conn.settimeout(TIMEOUT)
        data = b""
        while b"\n" not in data:
            chunk = conn.recv(1024)
            if not chunk:
                break
            data += chunk
        
        if EXPECTED in data:
            conn.sendall(REPLY)
            msg = data.decode(errors="replace").strip()
            with connections_lock:
                successful_connections.append(f"client{client_num}:{msg}")
        else:
            with connections_lock:
                failed_connections.append(f"client{client_num}:unexpected={repr(data)}")
    except socket.timeout:
        with connections_lock:
            failed_connections.append(f"client{client_num}:timeout")
    except Exception as e:
        with connections_lock:
            failed_connections.append(f"client{client_num}:error={str(e)}")
    finally:
        conn.close()

# Try IPv6 first (which usually supports IPv4 via dual-stack), then fall back to IPv4
s = None
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
        s = None
        if family == socket.AF_INET:
            # Both families failed
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
        thread = threading.Thread(target=handle_client, args=(conn, addr, i+1))
        thread.start()
        threads.append(thread)
    
    # Wait for all threads to complete
    for thread in threads:
        thread.join()
    
    # Report results
    if len(successful_connections) == NUM_CLIENTS and len(failed_connections) == 0:
        print("SERVER_OK:received={} clients:{}".format(
            NUM_CLIENTS, 
            ",".join(successful_connections)
        ))
        sys.exit(0)
    else:
        print("SERVER_FAIL:expected={} success={} failed={}:{}".format(
            NUM_CLIENTS,
            len(successful_connections),
            len(failed_connections),
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
