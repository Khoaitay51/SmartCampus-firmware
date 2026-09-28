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

def handle_client(client_sock, client_addr, port):
    print(f"[BRIDGE:{port}] Incoming connection from {client_addr}", flush=True)
    target_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        target_sock.connect(('127.0.0.1', port))
    except Exception as e:
        print(f"[BRIDGE:{port}] Failed to connect to Mosquitto 127.0.0.1:{port}: {e}", flush=True)
        client_sock.close()
        return

    t1 = threading.Thread(target=forward, args=(client_sock, target_sock), daemon=True)
    t2 = threading.Thread(target=forward, args=(target_sock, client_sock), daemon=True)
    t1.start()
    t2.start()

def start_listener(port, label):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('0.0.0.0', port))
        server.listen(32)
        print(f"[BRIDGE] {label} active on 0.0.0.0:{port} -> 127.0.0.1:{port}", flush=True)
    except Exception as e:
        print(f"[BRIDGE] Warning: Could not bind 0.0.0.0:{port}: {e}", flush=True)
        return

    while True:
        try:
            client_sock, client_addr = server.accept()
            threading.Thread(target=handle_client, args=(client_sock, client_addr, port), daemon=True).start()
        except Exception as e:
            print(f"[BRIDGE:{port}] Accept error: {e}", flush=True)

def main():
    print("==========================================================", flush=True)
    print("🚀 SmartCampus Dual MQTT Bridge (TCP & WebSocket)", flush=True)
    print("==========================================================", flush=True)

    t_mqtt = threading.Thread(target=start_listener, args=(1883, "MQTT TCP (ESP32)"), daemon=True)
    t_ws   = threading.Thread(target=start_listener, args=(9001, "MQTT WebSocket (Dashboard)"), daemon=True)

    t_mqtt.start()
    t_ws.start()

    import time
    while True:
        try:
            time.sleep(1)
        except KeyboardInterrupt:
            break

if __name__ == '__main__':
    main()
