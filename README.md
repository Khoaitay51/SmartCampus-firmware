# SmartCampus — ESP32 All-In-One Node Firmware (ESP-IDF)

Firmware tích hợp toàn bộ cảm biến, cơ cấu chấp hành và màn hình hiển thị cho **ESP32-D DevKitC WROOM** trong hệ thống **Smart Campus BMS**.

---

## 📌 Bảng Sơ Đồ Chân Cắm (Pinout Wiring Table)

Tất cả các chân được quy hoạch tối ưu, không xung đột chân nạp Boot/Flash, an toàn tuyệt đối với bộ chuyển đổi ADC1 khi bật Wi-Fi:

| STT | Thiết bị / Module | Chân thiết bị | Chân ESP32 | Ghi chú kỹ thuật |
|:---:|---|---|:---:|---|
| **1** | **RGB LED** | Chân Đỏ (R) | **GPIO 17** | PWM LEDC Ch 0 |
| | | Chân Xanh lá (G) | **GPIO 18** | PWM LEDC Ch 1 |
| | | Chân Xanh dương (B) | **GPIO 19** | PWM LEDC Ch 2 |
| | | GND / VCC | **GND / 3.3V** | Cấu hình `RGB_LED_COMMON_ANODE` trong `app_config.h` |
| **2** | **DHT22** | DATA | **GPIO 22** | Cần trở kéo lên (pull-up) 4.7kΩ - 10kΩ lên 3.3V |
| | | VCC / GND | **3.3V / GND** | Đo nhiệt độ & độ ẩm |
| **3** | **Cảm biến Khói MQ-2** | A0 (Analog Out) | **GPIO 34** | Kênh ADC1_CH6 (An toàn khi dùng Wi-Fi) |
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
| **9** | **Màn hình OLED SSD1306** | SDA | **GPIO 21** | Giao tiếp I2C |
| | | SCL | **GPIO 5** | Giao tiếp I2C |
| | | VCC / GND | **3.3V / GND** | |
| **10** | **Đầu đọc thẻ RFID RC522** | SDA (SS / CS) | **GPIO 27** | SPI Chip Select |
| | | SCK | **GPIO 26** | SPI Clock |
| | | MOSI | **GPIO 23** | SPI Master Out |
| | | MISO | **GPIO 25** | SPI Master In |
| | | RST | **GPIO 4** | Reset pin |
| | | 3.3V / GND | **3.3V / GND** | **LƯU Ý: Tuyệt đối cấp 3.3V, không cấp 5V** |

---

## ⚙️ Cấu hình Vai trò Node (Node Roles) & Mạng

Trong [`include/app_config.h`](include/app_config.h), chọn vai trò phù hợp cho bo mạch bằng cách đổi giá trị `CURRENT_NODE_ROLE`:

```c
#define ROLE_FULL_NODE          0   // 1 bo mạch duy nhất gánh toàn bộ (Cảm biến + OLED + Servo)
#define ROLE_IR_SENSOR_NODE     1   // Bo mạch 1: Chuyên cảm biến IR đếm người, DHT22, MQ-2, RC522
#define ROLE_OLED_DISPLAY_NODE  2   // Bo mạch 2: Chuyên nhận telemetry qua MQTT và hiển thị OLED (+ Servo)
#define ROLE_CORRIDOR_NODE      3   // Bo mạch 3: Node Hành lang - Quét thẻ RFID đăng ký thẻ chưa có UUID / Access control

#define CURRENT_NODE_ROLE       ROLE_IR_SENSOR_NODE
```

### Chi tiết Node Hành lang (`ROLE_CORRIDOR_NODE`):
- **Mục đích**: Đặt tại khu vực hành lang / sảnh sinh viên để quét thẻ RFID.
- **Nghiệp vụ**:
  - Khi quét thẻ chưa có trong hệ thống (`card_uid` chưa gắn `user_id`), ESP32 gửi gói tin lên topic `smartcampus/v1/card/registration/request` với trạng thái `pending`.
  - Màn hình OLED hiển thị: `UID: <card_uid>` kèm dòng thông báo `ST: CHO DUYET (PENDING)`.
  - Đèn RGB LED chuyển màu **Vàng cam**, còi Buzzer phát 2 tiếng beep ngắn xác nhận đã gửi yêu cầu.
  - Khi Admin phê duyệt (`APPROVED`) hoặc từ chối (`REJECTED`) trên hệ thống, Edge Gateway gửi tin nhắn phản hồi về topic `smartcampus/v1/card/registration/response/{mac_address}`:
    - `APPROVED`: OLED hiển thị tên người dùng và `ST: DA DUYET / OK`, LED đổi sang **Xanh lá**, còi kêu 1 tiếng beep ngắn.
    - `REJECTED`: OLED hiển thị `ST: TU CHOI`, LED đổi sang **Đỏ**, còi hú dài báo lỗi.

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
   - Trên Node Phòng học: Hiển thị chế độ phòng, Nhiệt độ/Độ ẩm, Chốt cửa, Mã thẻ gần nhất.
   - Trên Node Hành lang: Hiển thị giao diện đăng ký thẻ chuyên dụng (`=== CORRIDOR NODE ===`, `UID`, `ST: CHO DUYET`, `INFO`).
