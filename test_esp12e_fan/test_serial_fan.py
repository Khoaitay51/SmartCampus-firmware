#!/usr/bin/env python3
"""
Script Python kiểm tra điều khiển Bật/Tắt quạt qua Serial với ESP-12E / NodeMCU.
Yêu cầu: pip install pyserial (đã cài sẵn)
Chạy:
  python test_serial_fan.py [COM_PORT]
Ví dụ:
  python test_serial_fan.py COM3
"""

import sys
import time
import threading

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("❌ Chưa cài đặt thư viện pyserial! Cài đặt bằng: pip install pyserial")
    sys.exit(1)

def list_available_ports():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("⚠️  Không tìm thấy cổng COM nào đang cắm vào máy!")
        return []
    print("\nDanh sách cổng COM khả dụng:")
    for idx, p in enumerate(ports):
        print(f"  [{idx + 1}] {p.device} - {p.description}")
    return ports

def read_serial_loop(ser, stop_event):
    while not stop_event.is_set():
        try:
            line = ser.readline().decode('utf-8', errors='replace').rstrip()
            if line:
                print(f"\r[ESP12E] {line}")
                print(">> Nhập lệnh: ", end="", flush=True)
        except Exception:
            break

def run_auto_test_sequence(ser):
    print("\n========================================================")
    print("🧪 BẮT ĐẦU KỊCH BẢN KIỂM TRA TỰ ĐỘNG (DIAGNOSTIC TEST)")
    print("========================================================")
    
    steps = [
        ("TẮT QUẠT (Ban đầu)", "0\n", 2),
        ("BẬT QUẠT 100% CÔNG SUẤT", "1\n", 4),
        ("GIẢM TỐC ĐỘ XUỐNG 50% (PWM 512)", "p 512\n", 3),
        ("GIẢM TỐC ĐỘ XUỐNG 25% (PWM 256)", "p 256\n", 3),
        ("TĂNG TỐC ĐỘ LÊN 100% (PWM 1023)", "p 1023\n", 3),
        ("TẮT QUẠT HOÀN TOÀN", "0\n", 2),
    ]

    for desc, cmd, duration in steps:
        print(f"\n[BƯỚC] {desc} (Thời gian giữ: {duration}s)...")
        ser.write(cmd.encode('utf-8'))
        ser.flush()
        time.sleep(duration)

    print("\n✅ HOÀN TẤT KỊCH BẢN KIỂM TRA!")
    print("Chuyển về chế độ nhập lệnh thủ công...")

def main():
    port = None
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        ports = list_available_ports()
        if not ports:
            sys.exit(1)
        if len(ports) == 1:
            port = ports[0].device
            print(f"👉 Tự động chọn cổng duy nhất: {port}")
        else:
            choice = input("\nChọn số thứ tự cổng COM (hoặc nhập tên cổng COM, ví dụ COM3): ").strip()
            if choice.isdigit() and 1 <= int(choice) <= len(ports):
                port = ports[int(choice) - 1].device
            else:
                port = choice

    print(f"\n📡 Đang mở kết nối {port} ở tốc độ 115200 baud...")
    try:
        ser = serial.Serial(port, 115200, timeout=0.5)
        time.sleep(1.5)  # Chờ ESP reset sau khi mở DTR/RTS
    except Exception as e:
        print(f"❌ Không thể mở cổng {port}: {e}")
        sys.exit(1)

    print("✅ Đã kết nối thành công tới ESP-12E!")
    print("\nMenu điều khiển:")
    print("  1        : Bật quạt (100%)")
    print("  0        : Tắt quạt")
    print("  t        : Đảo trạng thái (Toggle)")
    print("  p <0-1023>: Điều chỉnh tốc độ băm xung PWM")
    print("  test     : Chạy kịch bản tự động test tốc độ")
    print("  auto     : Chuyển ESP sang chế độ tự bật 5s / tắt 5s")
    print("  q        : Thoát chương trình")
    print("--------------------------------------------------------\n")

    stop_event = threading.Event()
    reader_thread = threading.Thread(target=read_serial_loop, args=(ser, stop_event), daemon=True)
    reader_thread.start()

    # Gửi lệnh lấy menu
    ser.write(b"?\n")
    ser.flush()

    try:
        while True:
            cmd = input(">> Nhập lệnh: ").strip()
            if not cmd:
                continue
            if cmd.lower() in ('q', 'exit', 'quit'):
                break
            if cmd.lower() == 'test':
                run_auto_test_sequence(ser)
                continue
            
            ser.write((cmd + "\n").encode('utf-8'))
            ser.flush()
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        try:
            ser.close()
        except Exception:
            pass
        print("\n👋 Đã đóng kết nối cổng Serial.")

if __name__ == '__main__':
    main()
