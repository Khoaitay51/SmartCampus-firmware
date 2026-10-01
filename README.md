# SmartCampus — ESP32 All-In-One Node Firmware (ESP-IDF)

Firmware tích hợp toàn bộ cảm biến, cơ cấu chấp hành và màn hình hiển thị cho **ESP32-D DevKitC WROOM** trong hệ thống **Smart Campus BMS**.

---

## 📌 Bảng Sơ Đồ Chân Cắm (Pinout Wiring Table)

Tất cả các chân được quy hoạch tối ưu, không xung đột chân nạp Boot/Flash, an toàn tuyệt đối với bộ chuyển đổi ADC1 khi bật Wi-Fi:

| STT | Thiết bị / Module | Chân thiết bị | Chân ESP32 | Ghi chú kỹ thuật |
|:---:|---|---|:---:|---|
| **1** | **RGB LED** | Chân Đỏ (R) | **GPIO 17** | PWM LEDC Ch 0 |
| | | Chân Xanh lá (G) | **GPIO 26** | PWM LEDC Ch 1 *(Chuyển sang 26, tránh trùng VSPI SCK)* |
| | | Chân Xanh dương (B) | **GPIO 25** | PWM LEDC Ch 2 *(Chuyển sang 25, tránh trùng VSPI MISO)* |
| | | GND / VCC | **GND / 3.3V** | Cấu hình `RGB_LED_COMMON_ANODE` trong `app_config.h` |
| **2** | **DHT22 (Nhiệt/Ẩm)** | DATA | **GPIO 27** | Single-bus Data *(Chuyển sang 27, nhường GPIO 22 cho I2C)*, cần trở pull-up 4.7kΩ - 10kΩ |
| | | VCC / GND | **3.3V / GND** | Nguồn 3.3V ổn định |
| **3** | **Cảm biến Khói MQ-2** | A0 (Analog Out) | **GPIO 34** | Kênh ADC1_CH6 (An toàn tuyệt đối khi dùng Wi-Fi) |
| | | VCC / GND | **5V / GND** | Cần 5V để nung nóng sensor |
| **4** | **Cảm biến Không khí MQ-135** | A0 (Analog Out) | **GPIO 35** | Kênh ADC1_CH7 (Đo CO2 & AQI) |
| | | VCC / GND | **5V / GND** | Cần 5V để nung nóng sensor |
| **5** | **IR Sensor 1 (Ngoài cửa - IN)** | OUT | **GPIO 32** | Cảm biến hồng ngoại đếm người vào |
| | | VCC / GND | **3.3V hoặc 5V / GND** | |
| **6** | **IR Sensor 2 (Trong cửa - OUT)** | OUT | **GPIO 33** | Cảm biến hồng ngoại đếm người ra |
| | | VCC / GND | **3.3V hoặc 5V / GND** | |
| **7** | **Còi Buzzer** | I/O (+) | **GPIO 13** | Kêu bíp thẻ / báo cháy khẩn cấp |
| | | GND (-) | **GND** | |
| **8** | **Servo Motor SG90 (Cửa)** | Dây Cam (PWM) | **GPIO 16** | Tín hiệu PWM điều khiển chốt cửa |
| | | Dây Đỏ / Nâu | **5V / GND** | Nguồn 5V |
| **9** | **Quạt làm mát / thông gió (Fan)**| Control | **GPIO 14** | Điều khiển qua Transistor NPN / MOSFET / Relay |
| | | VCC / GND | **5V / GND** | Nguồn quạt 5V |
| **10** | **Màn hình OLED SSD1306** | SDA | **GPIO 21** | Giao tiếp I2C master mặc định của ESP32 |
| | | SCL | **GPIO 22** | Giao tiếp I2C master mặc định của ESP32 *(Chuyển sang 22)* |
| | | VCC / GND | **3.3V / GND** | Hiển thị trạng thái & telemetry |
| **11** | **Đầu đọc thẻ RFID RC522** | SDA (SS / CS) | **GPIO 5** | **VSPI Hardware CS** *(Chuẩn phần cứng, chống dội xung)* |
| | | SCK | **GPIO 18** | **VSPI Hardware SCK** (Xung clock 1 MHz) |
| | | MOSI | **GPIO 23** | **VSPI Hardware MOSI** (Nối thẳng MOSI $\rightarrow$ MOSI) |
| | | MISO | **GPIO 19** | **VSPI Hardware MISO** (Nối thẳng MISO $\rightarrow$ MISO, có pull-up) |
| | | IRQ | *(Bỏ trống)* | Không cần nối |
| | | GND | **GND** | Chân số 6 trên bo mạch RC522 |
| | | RST | **GPIO 4 hoặc 3.3V** | Chân số 7 (Khuyên cắm thẳng 3.3V để chip luôn ON) |
| | | 3.3V (VCC) | **3.3V** | Chân số 8 - **⚠️ LƯU Ý: Tuyệt đối cấp 3.3V, KHÔNG cấp 5V** |

---

## ⚙️ Cấu hình Vai trò Node (Node Roles) & Mạng

Hệ thống được quy hoạch gồm **2 Room Node** (Phòng 1 & Phòng 2) và **1 Corridor Node** (Node Hành lang):

Trong [`include/app_config.h`](include/app_config.h), chọn vai trò bo mạch cần nạp bằng cách đổi giá trị `CURRENT_NODE_ROLE`:

```c
#define ROLE_ROOM_NODE_1        1   // Bo mạch Phòng 1: Cảm biến (DHT22, MQ-2, MQ-135), IR Occupancy, OLED, RFID, Servo, Fan
#define ROLE_ROOM_NODE_2        2   // Bo mạch Phòng 2: Cảm biến (DHT22, MQ-2, MQ-135), IR Occupancy, OLED, RFID, Servo, Fan
#define ROLE_CORRIDOR_NODE      3   // Bo mạch Hành lang: Quét RFID đăng ký thẻ + OLED + RGB LED + Buzzer

// 👉 Chọn vai trò muốn nạp cho ESP32 hiện tại:
#define CURRENT_NODE_ROLE       ROLE_ROOM_NODE_1
```

### Chi tiết các Node trong hệ thống:
1. **Room Node 1 (`ROLE_ROOM_NODE_1`)**:
   - Vị trí: Phòng học 1 (Room ID mặc định: `11111111-1111-1111-1111-111111111111`).
   - Màn hình OLED: Header đảo màu `ROOM 1    W:OK M:OK`, Chế độ phòng, Cửa, Quạt, Nhiệt độ/Độ ẩm, Khói, Số người, 2 mắt IR, Thẻ RFID điểm danh.
   - Ngoại vi: DHT22, MQ-2, MQ-135, IR vào/ra, RC522, OLED SSD1306, SG90 Servo, Quạt DC, RGB LED, Buzzer.
2. **Room Node 2 (`ROLE_ROOM_NODE_2`)**:
   - Vị trí: Phòng học 2 (Room ID mặc định: `22222222-2222-2222-2222-222222222222`).
   - Màn hình OLED: Header đảo màu `ROOM 2    W:OK M:OK`, telemetry và trạng thái tương tự Room 1.
   - Ngoại vi: Đầy đủ cảm biến và cơ cấu chấp hành phòng học.
3. **Corridor Node (`ROLE_CORRIDOR_NODE`)**:
   - Vị trí: Sảnh sinh viên / Khu vực hành lang phục vụ đăng ký & xác thực thẻ RFID.
   - Màn hình OLED: Header đảo màu `CORRIDOR  W:OK M:OK`, giao diện đăng ký thẻ chuyên dụng (`UID`, `ST: CHO DUYET / DA DUYET / TU CHOI`, `INFO`, `MAC / IP`).
   - Nghiệp vụ: Khi quét thẻ chưa gắn sinh viên, ESP32 gửi request lên `smartcampus/v1/card/registration/request`. Khi Admin duyệt hoặc từ chối, ESP32 nhận phản hồi qua topic `smartcampus/v1/card/registration/response/{mac_address}`, OLED hiển thị kết quả kèm phản hồi LED và còi.

---

## 🚀 Hướng dẫn Biên dịch & Nạp Firmware

### Cách 1: Sử dụng PlatformIO
```bash
cd /home/user_kma_chinh/SmartCampus-firmware
pio run -t upload -t monitor
```

### Cách 2: Sử dụng ESP-IDF Native CLI
```bash
cd /home/user_kma_chinh/SmartCampus-firmware
idf.py set-target esp32
idf.py -p COMx flash monitor   # Trên Windows (thay COMx tương ứng)
```

---

## 📡 Chức năng & Luồng hoạt động trong hệ thống

1. **Auto Provisioning (FR-DM-01, FR-DM-02)**: Khi khởi động, ESP32 gửi MAC address lên `smartcampus/v1/device/provision/request`, nhận `room_id` và lưu vào NVS.
2. **Đo lường Môi trường (FR-SD-01, FR-SD-03, FR-SD-05)**: Định kỳ mỗi **2 giây**, đo DHT22 (Nhiệt độ, Độ ẩm) + MQ-2 (Khói) + MQ-135 (CO2, AQI) gửi lên `smartcampus/v1/telemetry/room/{room_id}/environment`.
3. **Đếm người ra/vào (FR-SD-02)**:
   - Bước qua `IR_IN` trước rồi `IR_OUT` -> Gửi sự kiện `IN` lên `smartcampus/v1/telemetry/room/{room_id}/occupancy`.
   - Bước qua `IR_OUT` trước rồi `IR_IN` -> Gửi sự kiện `OUT` lên `smartcampus/v1/telemetry/room/{room_id}/occupancy`.
4. **Điểm danh & Đăng ký thẻ RFID RC522 (FR-SD-04, FR-RF-01)**:
   - Tại phòng học (`ROLE_FULL_NODE` / `ROLE_IR_SENSOR_NODE`): Quẹt thẻ sinh viên/giảng viên -> Gửi mã thẻ lên `smartcampus/v1/event/room/{room_id}/rfid` điểm danh.
   - Tại hành lang (`ROLE_CORRIDOR_NODE`): Quẹt thẻ -> Gửi yêu cầu đăng ký lên `smartcampus/v1/card/registration/request` với trạng thái `pending`.
5. **Đồng bộ trạng thái FSM (FR-AC-01, FR-AC-02, FR-AC-04)**:
   - `SAVING`: LED tắt (`#000000`), Servo khóa cửa.
   - `SELF_STUDY`: LED xanh lam nhạt (`#66CCFF`), Servo mở cửa.
   - `LECTURE`: LED trắng sáng (`#FFFFFF`), Servo mở cửa.
   - `EXAM`: LED hổ phách (`#FFBF00`), Servo khóa cửa, Buzzer kêu 2 tiếng bíp bắt đầu thi.
   - `SUSPECTED`: LED cam thở (`#FF8C00` Breathe).
   - `EMERGENCY`: LED đỏ chớp nháy dồn dập (`#FF0000` Strobe), Servo lập tức mở toang cửa, Buzzer hú liên tục.
6. **Màn hình OLED SSD1306 (FR-AC-05)**:
   - **Giao diện Boot**: Báo từng bước khởi động phần cứng, kết nối Wi-Fi (kèm IP nhận được), đồng bộ giờ SNTP và MQTT.
   - **Giao diện Room Node**: Header đảo màu thanh trạng thái (`[ROOM X] W:OK M:OK`), Chế độ phòng (`MODE: LECTURE`), Trạng thái cửa (`D:LOCK/OPEN`), Quạt, Cảm biến Nhiệt/Ẩm, Khói/Gas, Số người (`OCC: X [IR:1,1]`), Mã thẻ RFID vừa điểm danh.
   - **Giao diện Corridor Node**: Header đảo màu (`[CORRIDOR] W:OK M:OK`), Trạng thái đăng ký (`CHO DUYET / DA DUYET / TU CHOI`), Mã thẻ `UID`, Tên sinh viên `INFO`, Địa chỉ MAC / IP thiết bị.
