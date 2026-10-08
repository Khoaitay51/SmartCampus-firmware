# Hướng dẫn Kiểm tra Bật/Tắt Quạt DC bằng Module Relay 5V / Transistor qua ESP-12E (ESP8266)

Thư mục này chứa đầy đủ mã nguồn và công cụ để bạn kiểm tra đóng/ngắt quạt làm mát DC (5V hoặc 12V) sử dụng **Module Relay 5V (Active LOW)** hoặc **Transistor NPN / MOSFET** điều khiển từ bo mạch **ESP-12E / NodeMCU ESP8266**.

---

## 1. Sơ đồ Đấu nối Phần cứng (Hardware Wiring)

### 📌 Chân GPIO sử dụng
* **Chân điều khiển Quạt:** `D1` (tương ứng **GPIO 5** trên ESP8266) - *Hoặc bất kỳ chân nào trong danh sách: `D1`, `D2`, `D5`, `D6`, `D7`*.
  > **⚠️ Lưu ý sống còn về GPIO trên ESP8266:**  
  > Luôn ưu tiên dùng **D1 (GPIO 5)** hoặc **D2 (GPIO 4)**.  
  > **TUYỆT ĐỐI KHÔNG DÙNG** các chân `D3 (GPIO 0)`, `D4 (GPIO 2)`, `D8 (GPIO 15)` cho chân điều khiển, vì đây là các chân cấu hình Boot (strapping pins) của ESP8266.

---

### 🔌 Cách 1: Đấu nối Module Relay 5V (Khuyên dùng - Phổ biến nhất)

```text
  ESP-12E / NodeMCU                    Module Relay 5V (Songle 1-Ch)
┌──────────────────┐                  ┌──────────────────────────────┐
│                  │                  │                              │
│         5V (VIN) ├──────────────────┤ VCC                          │
│              GND ├──────────────────┤ GND                          │
│      D1 (GPIO 5) ├──────────────────┤ IN (Tín hiệu Active LOW)     │
│                  │                  │                              │
└──────────────────┘                  │  [COM] ─── Nguồn +5V         │
                                      │  [NO ] ─── Dây Dương (+) Quạt│
                                      │  [NC ] (Bỏ trống)            │
                                      └──────────────────────────────┘
                                      Dây Âm (-) Quạt ──── GND (Nguồn 5V)
```

* **Cơ chế hoạt động:**
  * Relay kích mức thấp (**Active LOW**): Khi ESP8266 xuất `LOW (0V)`, cuộn hút rơ-le đóng tiếp điểm `COM` nối sang `NO` $\rightarrow$ Quạt được cấp điện **BẬT**.
  * Khi ESP8266 xuất `HIGH (3.3V)`, rơ-le ngắt tiếp điểm `COM` - `NO` $\rightarrow$ Quạt ngắt điện **TẮT**.

---

### 🔌 Cách 2: Sơ đồ mạch dùng Transistor NPN (S8050, 2N2222, TIP120, BD139)

```text
               +5V (hoặc +12V Nguồn Quạt)
                     │
                     ├────────────────────────┐
                     │                        │
                 ┌───┴───┐               ┌────┴───┐
                 │       │               │        │  Diode 1N4007
                 │  QUẠT │               │   ▲    │  (Flyback Diode)
                 │  DC   │               │   │    │  (Vạch bạc nối vào +)
                 │       │               │   │    │
                 └───┬───┘               └────┬───┘
                     │                        │
                     ├────────────────────────┘
                     │
                 [Collector (C)]
                     │
              ┌──────┴──────┐
GPIO 5 (D1) ──┤ R = 1kΩ  [B]│ Transistor NPN
              └──────┬──────┘ (S8050 / 2N2222 / TIP120)
                     │
                 [Emitter (E)]
                     │
                     ├──────────────────────── GND Nguồn Quạt
                     │
                    GND (ESP-12E / NodeMCU)  <-- BẮT BUỘC NỐI CHUNG GND
```

#### Chi tiết linh kiện Transistor:
1. **Transistor NPN:** **S8050** (max ~500mA), **2N2222** (~600mA), **TIP120** (~5A).
   * **B (Base):** Nối qua điện trở $1\text{k}\Omega$ vào chân **D1 (GPIO 5)**.
   * **C (Collector):** Nối vào **cực âm (- / dây đen)** của quạt.
   * **E (Emitter):** Nối vào **GND**.
2. **Điện trở Base ($1\text{k}\Omega$):** Hạn dòng chân GPIO.
3. **Diode 1N4007 (hoặc 1N4148):** Dập xung ngược cảm ứng bảo vệ transistor.
4. **Nối mass chung (Common GND):** Bắt buộc nối GND nguồn ngoài vào GND ESP8266.

---

### 🔌 Cách 3: Nếu dùng Module MOSFET (IRLZ44N / AOD4184 / Module FR120N)
Nếu dùng Module MOSFET có sẵn trên thị trường:
* Chân **TRIG / PWM**: Nối vào **D1 (GPIO 5)** của ESP-12E.
* Chân **GND**: Nối vào GND của ESP-12E.
* Chân **V+ / V-**: Cấp nguồn quạt (5V hoặc 12V).
* Chân **LOAD+ / LOAD-**: Nối 2 dây quạt.

---

## 2. Cách Nạp Code vào Bo mạch ESP-12E

### Cách 1: Nạp trực tiếp từ WSL2 bằng `./flash.sh` (Khuyên dùng)
Bạn chỉ cần mở terminal WSL2 và chạy lệnh sau (tương tự như nạp ESP32):

```bash
# Nạp cho cổng COM (mặc định COM6):
./test_esp12e_fan/flash.sh COM6

# Hoặc cd vào thư mục:
cd test_esp12e_fan
./flash.sh COM6
```
* Script sẽ tự động dùng **PlatformIO Core (đã cài trong WSL2)** để biên dịch mã nguồn sang `firmware.bin` trong chưa đầy 1 giây.
* Sau đó tự động gọi `esptool` nạp trực tiếp qua cổng COM trên Windows.

---

### Cách 2: Nạp bằng Arduino IDE
1. Mở file [test_esp12e_fan.ino](file:///home/user_kma_chinh/SmartCampus-firmware/test_esp12e_fan/test_esp12e_fan.ino) trong Arduino IDE.
2. Vào **Tools** -> **Board** -> Chọn **NodeMCU 1.0 (ESP-12E Module)** (hoặc *Generic ESP8266 Module*).
3. Vào **Tools** -> **Port** -> Chọn cổng COM tương ứng của bo mạch.
4. Tốc độ nạp (Upload Speed): `115200` (hoặc `921600`).
5. Bấm nút **Upload (Nạp)** (mũi tên sang phải).

---

### Cách 3: Nạp bằng PlatformIO (VS Code)
Thư mục này đã có sẵn file cấu hình [`platformio.ini`](file:///home/user_kma_chinh/SmartCampus-firmware/test_esp12e_fan/platformio.ini) và mã nguồn [`src/main.cpp`](file:///home/user_kma_chinh/SmartCampus-firmware/test_esp12e_fan/src/main.cpp).
1. Mở folder `test_esp12e_fan` trong VS Code có cài extension PlatformIO.
2. Nhấn biểu tượng **PlatformIO** -> chọn **Upload**.

---

## 3. Cách Kiểm tra (Testing)

### Chế độ 1: Tự động Bật/Tắt (Mặc định khi cắm điện)
Ngay khi cấp nguồn:
* Quạt sẽ **tự động BẬT 5 giây**, sau đó **TẮT 5 giây**, lặp đi lặp lại tuần hoàn.
* Đèn LED xanh trên module ESP-12E (`LED_BUILTIN`) sẽ sáng khi quạt BẬT và tắt khi quạt TẮT.

---

### Chế độ 2: Điều khiển qua Serial Monitor (115200 baud)
Mở Serial Monitor (tốc độ **115200 baud**, chọn `Newline` hoặc `Both NL & CR`), gửi các lệnh sau:

| Lệnh | Ý nghĩa |
|---|---|
| `1` hoặc `ON` | Bật quạt 100% công suất |
| `0` hoặc `OFF` | Tắt quạt hoàn toàn |
| `t` hoặc `TOGGLE` | Đảo trạng thái Bật $\leftrightarrow$ Tắt |
| `p <0-1023>` | Điều chỉnh tốc độ gió bằng PWM (Ví dụ: `p 512` là 50%, `p 1023` là 100%) |
| `auto` | Chuyển về chế độ Tự động (5s bật / 5s tắt) |
| `manual` | Giữ nguyên trạng thái để chờ lệnh thủ công |
| `?` hoặc `HELP` | Xem menu trợ giúp |

---

### Chế độ 3: Chạy Script Test tương tác trực tiếp từ WSL2 (`./test.sh`)

Do cổng `COM8` nằm trên Windows, để tương tác trực tiếp từ terminal WSL2, bạn chỉ cần gõ:

```bash
# Chạy script test tương tác (tự động nhận diện COM8):
./test_esp12e_fan/test.sh

# Hoặc mở Serial Monitor thông thường:
./test_esp12e_fan/monitor.sh
```

* Script sẽ tự động kết nối tới `COM8` qua Windows Python và hiển thị giao diện nhập lệnh ngay trong terminal WSL2.
* Bạn có thể gõ các lệnh: `1`, `0`, `t`, `p 512`, `auto`,...
* Đặc biệt, gõ lệnh `test` để chạy chuỗi kiểm tra tự động các cấp tốc độ:
  `Tắt -> Bật 100% (4s) -> Giảm 50% (3s) -> Giảm 25% (3s) -> Tăng 100% (3s) -> Tắt`.

*(Nếu muốn chạy từ Windows: bạn chỉ cần click đúp vào file `run_test.bat` trong thư mục `test_esp12e_fan`)*.
