#pragma once

#include <stdint.h>

// ---------------- Node Roles (Cấu hình vai trò cho từng bo mạch) ----------------
#define ROLE_FULL_NODE          0   // 1 bo mạch duy nhất gánh toàn bộ (Cảm biến + OLED + Servo)
#define ROLE_IR_SENSOR_NODE     1   // Bo mạch 1: Chuyên cảm biến IR đếm người, DHT22, MQ-2, RC522
#define ROLE_OLED_DISPLAY_NODE  2   // Bo mạch 2: Chuyên nhận telemetry qua MQTT và hiển thị OLED (+ Servo)
#define ROLE_CORRIDOR_NODE      3   // Bo mạch 3: Node hành lang - Quét RFID đăng ký thẻ chưa có UUID / Access control

// 👉 ĐANG CẤU HÌNH CHO BO MẠCH (ROLE_FULL_NODE / ROLE_IR_SENSOR_NODE / ROLE_OLED_DISPLAY_NODE / ROLE_CORRIDOR_NODE):
#define CURRENT_NODE_ROLE       ROLE_IR_SENSOR_NODE

#if CURRENT_NODE_ROLE == ROLE_OLED_DISPLAY_NODE
  #define MQTT_USERNAME         "smartcampus_oled"
  #define MQTT_CLIENT_PREFIX    "ESP32_OLED"
#elif CURRENT_NODE_ROLE == ROLE_IR_SENSOR_NODE
  #define MQTT_USERNAME         "smartcampus_ir"
  #define MQTT_CLIENT_PREFIX    "ESP32_IR"
#elif CURRENT_NODE_ROLE == ROLE_CORRIDOR_NODE
  #define MQTT_USERNAME         "smartcampus_corridor"
  #define MQTT_CLIENT_PREFIX    "ESP32_CORRIDOR"
#else
  #define MQTT_USERNAME         "smartcampus"
  #define MQTT_CLIENT_PREFIX    "ESP32_FULL"
#endif

// ---------------- Wi-Fi & MQTT Broker Config ----------------
// Giá trị mặc định khi chưa cấu hình qua NVS.
// Sử dụng app_nvs_get_wifi_ssid() / app_nvs_get_mqtt_uri() để đọc giá trị runtime.
#define DEFAULT_WIFI_SSID           "Trung Tam TT-TV"
#define DEFAULT_WIFI_PASSWORD       "12345679"
#define DEFAULT_MQTT_BROKER_URI     "mqtt://192.168.22.17:1883"
#define DEFAULT_MQTT_PASSWORD       "123456"

// Phòng mặc định (Phòng 402 đã có sẵn trong Docker TimescaleDB)
#define DEFAULT_ROOM_ID             "11111111-1111-1111-1111-111111111111"

// ---------------- Hardware Pinout ----------------
// RGB LED (LEDC Channels 0, 1, 2)
#define PIN_LED_RED                 17
#define PIN_LED_GREEN               18
#define PIN_LED_BLUE                19
#define RGB_LED_COMMON_ANODE        0       // 0: Cathode chung (GND), 1: Anode chung (3.3V)

// DHT22 (Nhiệt độ & Độ ẩm)
#define PIN_DHT22                   22

// MQ-2 (Khói) & MQ-135 (Air Quality / CO2) - Sử dụng ADC1 (Không bị ngắt khi dùng Wi-Fi)
#define PIN_MQ2_ADC                 34      // ADC1_CHANNEL_6
#define PIN_MQ135_ADC               35      // ADC1_CHANNEL_7

// Cặp cảm biến hồng ngoại (IR Occupancy: đếm người vào/ra)
#define PIN_IR_IN                   32      // Cảm biến đặt phía ngoài cửa
#define PIN_IR_OUT                  33      // Cảm biến đặt phía trong cửa

// Servo cửa SG90 (LEDC Channel 3)
#define PIN_SERVO_DOOR              16

// Quạt làm mát / thông gió DC 5V (2 dây: VCC + GND, điều khiển qua Transistor NPN/MOSFET)
#define PIN_FAN                     14
#define FAN_ACTIVE_HIGH             1       // 1: Mức cao (Transistor NPN / MOSFET), 0: Mức thấp (Relay Active LOW)
#define FAN_2WIRE_GPIO              1       // 1: Quạt 2 dây (GPIO on/off), 0: Quạt 3-4 dây (PWM speed control via LEDC)

// Còi Buzzer
#define PIN_BUZZER                  13
#define BUZZER_ACTIVE_LOW           1       // 1: Kích mức thấp (Active LOW - phổ biến), 0: Kích mức cao (Active HIGH)

// Màn hình OLED SSD1306 (I2C)
#define PIN_OLED_SDA                21
#define PIN_OLED_SCL                5
#define OLED_I2C_ADDR               0x3C

// Đầu đọc thẻ RFID RC522 (SPI)
#define PIN_RC522_SCK               26
#define PIN_RC522_MOSI              23
#define PIN_RC522_MISO              25
#define PIN_RC522_CS                27      // Chân SDA/SS trên module RC522
#define PIN_RC522_RST               4

// ---------------- Timing Intervals ----------------
#define SENSOR_READ_INTERVAL_MS     2000    // Đọc & publish DHT22 + Khói mỗi 2s (FR-SD-01, FR-SD-03)
#define HEARTBEAT_INTERVAL_SEC      30      // Gửi heartbeat mỗi 30s (FR-DM-04)

// ---------------- NVS Storage Keys ----------------
#define NVS_NAMESPACE               "smartcampus"
#define NVS_KEY_ROOM_ID             "room_id"
#define NVS_KEY_DEVICE_ID           "device_id"
#define NVS_KEY_WIFI_SSID           "wifi_ssid"
#define NVS_KEY_WIFI_PASS           "wifi_pass"
#define NVS_KEY_MQTT_URI            "mqtt_uri"
#define NVS_KEY_MQTT_PASS           "mqtt_pass"

// ---------------- MQTT Topics Namespace ----------------
#define TOPIC_PROVISION_REQUEST     "smartcampus/v1/device/provision/request"
#define TOPIC_PROVISION_RESPONSE    "smartcampus/v1/device/provision/response/%s"
#define TOPIC_DEVICE_STATUS         "smartcampus/v1/device/%s/status"
#define TOPIC_DEVICE_HEARTBEAT      "smartcampus/v1/device/%s/heartbeat"
#define TOPIC_DEVICE_COMMAND        "smartcampus/v1/command/device/%s"
#define TOPIC_ROOM_TELEMETRY_ENV    "smartcampus/v1/telemetry/room/%s/environment"
#define TOPIC_ROOM_TELEMETRY_OCC    "smartcampus/v1/telemetry/room/%s/occupancy"
#define TOPIC_ROOM_EVENT_RFID       "smartcampus/v1/event/room/%s/rfid"
#define TOPIC_ROOM_STATE            "smartcampus/v1/room/%s/state"
#define TOPIC_ROOM_COMMAND          "smartcampus/v1/command/room/%s"
#define TOPIC_COMMAND_ACK           "smartcampus/v1/ack/device/%s/command/%s"
#define TOPIC_CARD_REG_REQUEST      "smartcampus/v1/card/registration/request"
#define TOPIC_CARD_REG_RESPONSE     "smartcampus/v1/card/registration/response/%s"
