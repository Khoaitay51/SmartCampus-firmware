import socket
import threading
import sys

def forward(src, dst):
    try:
        while True:
            data = src.recv(4096)
            if not data:
                break
            dst.sendall(data)
    except Exception:
        pass
    finally:
        try:
            src.close()
        except Exception:
            pass
        try:
            dst.close()
        except Exception:
            pass

def handle_client(client_sock, client_addr):
    print(f"[BRIDGE] Incoming connection from {client_addr}", flush=True)
    target_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        target_sock.connect(('127.0.0.1', 1883))
    except Exception as e:
        print(f"[BRIDGE] Failed to connect to Mosquitto 127.0.0.1:1883: {e}", flush=True)
        client_sock.close()
        return

    t1 = threading.Thread(target=forward, args=(client_sock, target_sock), daemon=True)
    t2 = threading.Thread(target=forward, args=(target_sock, client_sock), daemon=True)
    t1.start()
    t2.start()

def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('192.168.22.43', 1883))
        server.listen(16)
        print("[BRIDGE] MQTT Proxy active on 192.168.22.43:1883 -> 127.0.0.1:1883", flush=True)
    except Exception as e:
        print(f"[BRIDGE] Fatal bind error: {e}", flush=True)
        sys.exit(1)

    while True:
        try:
            client_sock, client_addr = server.accept()
            handle_client(client_sock, client_addr)
        except Exception as e:
            print(f"[BRIDGE] Accept error: {e}", flush=True)

if __name__ == '__main__':
    main()
