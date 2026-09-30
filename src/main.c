#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "app_config.h"
#include "rc522.h"
#include "buzzer.h"
#include "rgb_led.h"
#include "oled_ssd1306.h"

// ===================================================================================
// 🔧 CẤU HÌNH CHẾ ĐỘ TEST (TEST MODE CONFIGURATION):
// - Đặt 1: Chế độ test chuyên biệt RC522 (Firmware tạm để test phần cứng độc lập)
// - Đặt 0: Chế độ chạy toàn bộ hệ thống SmartCampus hoàn chỉnh (Production mode)
// ===================================================================================
#define TEST_RC522_FIRMWARE  1

#if !TEST_RC522_FIRMWARE

// Include các module phục vụ chế độ Production
#include "app_nvs.h"
#include "app_wifi.h"
#include "app_sntp.h"
#include "dht22.h"
#include "mq_sensor.h"
#include "servo_door.h"
#include "fan_control.h"
#include "app_mqtt.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "=== SMARTCAMPUS ESP32 ALL-IN-ONE NODE BOOTING ===");

    // 1. NVS Storage
    ESP_ERROR_CHECK(app_nvs_init());

    // 2. Khoi tao Actuators & Hien thi
    rgb_led_init();
    rgb_led_set_color(40, 40, 0); // Vang: Dang khoi dong
    buzzer_init();
    servo_door_init();
    fan_control_init();
    oled_init();

#if CURRENT_NODE_ROLE != ROLE_OLED_DISPLAY_NODE
    // 3. Khoi tao RFID RC522 NGAY LAP TUC (hoat dong doc lap, khong phu thuoc Wi-Fi/MQTT)
    ESP_LOGI(TAG, "Initializing RC522 RFID reader...");
    rc522_init(app_mqtt_on_rfid_card_scanned);
#endif

#if CURRENT_NODE_ROLE != ROLE_OLED_DISPLAY_NODE && CURRENT_NODE_ROLE != ROLE_CORRIDOR_NODE
    // 4. Khoi tao Sensors (Chi can tren Sensor / IR hoac Full Node)
    dht22_init(PIN_DHT22);
    mq_sensor_init();
#endif

#if CURRENT_NODE_ROLE == ROLE_CORRIDOR_NODE
    oled_display_corridor(NULL, "SAN SANG", "QUET THE DANG KY");
#endif

    // 4. Ket noi Wi-Fi
    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    app_wifi_init_sta();
    if (app_wifi_wait_connected(6000)) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully!");

        // 5. F3 fix: Dong bo thoi gian thuc qua SNTP (sau khi co Wi-Fi)
        app_sntp_init();
        app_sntp_wait_synced(10000); // Cho toi da 10s de sync thoi gian
    } else {
        ESP_LOGW(TAG, "Wi-Fi not connected yet! SNTP will sync later when Wi-Fi is available.");
    }

    // 6. Khoi dong MQTT va cac Peripheral tasks (RFID, IR, Telemetry)
    app_mqtt_start();

    ESP_LOGI(TAG, "=== SYSTEM ALL-IN-ONE OPERATIONAL ===");
}

#else

// ===================================================================================
// 🎯 FIRMWARE TEST CHUYÊN BIỆT CHO ĐẦU ĐỌC THẺ RFID RC522
// ===================================================================================
static const char *TAG = "RC522_TEST";
static uint32_t s_tap_count = 0;

static const char *rc522_version_name(uint8_t ver) {
    switch (ver) {
        case 0x92: return "NXP MFRC522 v2.0 (Chinh hang)";
        case 0x91: return "NXP MFRC522 v1.0 (Chinh hang)";
        case 0x12: return "MFRC522 v2.0 Clone (Module xanh pho bien - Hoat dong tot)";
        case 0x88: return "FM17522 Compatible";
        case 0x00: return "KHONG NHAN TIN HIEU (0x00 - Kiem tra nguon/SPI/day noi)";
        case 0xFF: return "DUONG TRUYEN LOI (0xFF - MISO treo cao / dut day)";
        default:   return "PCD Compatible Unknown";
    }
}

static void test_on_rfid_card_scanned(const char *card_uid) {
    s_tap_count++;
    
    // Parse UID thanh dang Hex co dau cach & So nguyen
    unsigned int b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    sscanf(card_uid, "%02x%02x%02x%02x", &b0, &b1, &b2, &b3);
    uint32_t uid_dec = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | (uint32_t)b3;

    ESP_LOGI(TAG, "****************************************************************");
    ESP_LOGI(TAG, "💳 [QUẸT THẺ #%lu] PHÁT HIỆN THẺ RFID THÀNH CÔNG!", (unsigned long)s_tap_count);
    ESP_LOGI(TAG, "   👉 Chuỗi Hex   : %s", card_uid);
    ESP_LOGI(TAG, "   👉 Byte Hex    : %02X %02X %02X %02X", b0, b1, b2, b3);
    ESP_LOGI(TAG, "   👉 Số thập phân : %lu", (unsigned long)uid_dec);
    ESP_LOGI(TAG, "****************************************************************");

    // Phản hồi phần cứng: Còi kêu 1 tiếng ngắn + LED nháy Xanh lá + OLED cập nhật
    buzzer_play(BUZZER_PATTERN_SHORT);
    rgb_led_set_color(0, 80, 0); // Xanh lá

    char count_str[32];
    snprintf(count_str, sizeof(count_str), "LAN QUET: #%lu", (unsigned long)s_tap_count);
    oled_display_corridor(card_uid, "THANH CONG!", count_str);

    vTaskDelay(pdMS_TO_TICKS(400));
    rgb_led_set_color(0, 30, 80); // Trở về Xanh dương sẵn sàng
}

void app_main(void) {
    ESP_LOGI(TAG, "================================================================");
    ESP_LOGI(TAG, "⚡ FIRMWARE TEST CHUYÊN BIỆT CHO RFID RC522 (ESP-IDF / SPI) ⚡");
    ESP_LOGI(TAG, "================================================================");
    ESP_LOGI(TAG, "📌 SƠ ĐỒ ĐẤU DÂY CHUẨN ĐÃ QUY HOẠCH (KHÔNG XUNG ĐỘT):");
    ESP_LOGI(TAG, "   [RC522]  SDA (CS) --> GPIO %-2d | SCK  --> GPIO %-2d", PIN_RC522_CS, PIN_RC522_SCK);
    ESP_LOGI(TAG, "            MOSI     --> GPIO %-2d | MISO --> GPIO %-2d", PIN_RC522_MOSI, PIN_RC522_MISO);
    ESP_LOGI(TAG, "            RST      --> GPIO %-2d (hoặc 3.3V) | VCC: 3.3V, GND: GND", PIN_RC522_RST);
    ESP_LOGI(TAG, "   [OLED]   SDA      --> GPIO %-2d | SCL  --> GPIO %-2d (I2C chuẩn)", PIN_OLED_SDA, PIN_OLED_SCL);
    ESP_LOGI(TAG, "   [RGB]    R: GPIO %d | G: GPIO %d | B: GPIO %d", PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE);
    ESP_LOGI(TAG, "   [BUZZER] IO       --> GPIO %-2d", PIN_BUZZER);
    ESP_LOGI(TAG, "================================================================");

    // 1. Khởi tạo ngoại vi phản hồi
    rgb_led_init();
    rgb_led_set_color(60, 60, 0); // Vàng: Đang kiểm tra
    buzzer_init();
    oled_init();
    oled_display_corridor(NULL, "DANG KHOI DONG", "KIEM TRA RC522...");

    vTaskDelay(pdMS_TO_TICKS(200));

    // 2. Khởi tạo RC522
    ESP_LOGI(TAG, ">> Đang khởi tạo giao thức SPI và cảm biến RC522...");
    rc522_init(test_on_rfid_card_scanned);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 3. Kiểm tra thanh ghi Version
    uint8_t rx0 = 0, rx1 = 0;
    uint8_t ver = rc522_get_raw_probe(&rx0, &rx1);
    ESP_LOGI(TAG, ">> Kết quả đọc VersionReg (0x37): rx0=0x%02X, rx1=0x%02X => Ver: 0x%02X (%s)",
             rx0, rx1, ver, rc522_version_name(ver));

    // Nếu đọc ra 0x00 hoặc 0xFF => Lỗi phần cứng, lỏng dây
    while (ver == 0x00 || ver == 0xFF) {
        rgb_led_set_color(100, 0, 0); // Đỏ: Báo động lỗi
        buzzer_play(BUZZER_PATTERN_LONG);
        oled_display_corridor(NULL, "LOI RC522!", "KIEM TRA DAY NOI");

        ESP_LOGE(TAG, "❌ LỖI: Chưa nhận được RC522 (Version: 0x%02X | Raw rx0=0x%02X, rx1=0x%02X)!", ver, rx0, rx1);
        ESP_LOGE(TAG, "👉 BƯỚC KHẮC PHỤC:");
        ESP_LOGE(TAG, "   1. Kiểm tra chân VCC và RST đã nối vào 3.3V chưa (Đèn LED đỏ trên RC522 có sáng không?).");
        ESP_LOGE(TAG, "   2. Kiểm tra hàng chân header của module RC522 đã được HÀN CHÌ chắc chưa.");
        ESP_LOGE(TAG, "   3. Kiểm tra dây cắm: CS=GPIO%d, SCK=GPIO%d, MOSI=GPIO%d, MISO=GPIO%d",
                 PIN_RC522_CS, PIN_RC522_SCK, PIN_RC522_MOSI, PIN_RC522_MISO);
        ESP_LOGE(TAG, "   4. Đang tự động kiểm tra lại sau mỗi 2 giây...");

        vTaskDelay(pdMS_TO_TICKS(2000));
        ver = rc522_get_raw_probe(&rx0, &rx1);
    }

    // Kết nối thành công
    ESP_LOGI(TAG, "✅ RC522 HOẠT ĐỘNG HOÀN HẢO! Chip version: 0x%02X (%s)", ver, rc522_version_name(ver));
    rc522_dump_registers();

    // Còi bíp nhẹ 2 tiếng + LED Xanh dương báo sẵn sàng
    buzzer_play(BUZZER_PATTERN_DOUBLE);
    rgb_led_set_color(0, 30, 80); // Xanh dương
    char ver_str[32];
    snprintf(ver_str, sizeof(ver_str), "VER: 0x%02X - OK", ver);
    oled_display_corridor(NULL, "SAN SANG QUET", ver_str);

    ESP_LOGI(TAG, "================================================================");
    ESP_LOGI(TAG, "🎯 HỆ THỐNG SẴN SÀNG! ĐƯA THẺ RFID VÀO GẦN MODULE (1 - 3 CM)...");
    ESP_LOGI(TAG, "================================================================");

    // 4. Vòng lặp giám sát (Heartbeat)
    uint32_t loop_count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        loop_count++;

        uint8_t current_ver = rc522_get_version();
        if (current_ver == 0x00 || current_ver == 0xFF) {
            ESP_LOGW(TAG, "⚠️ CẢNH BÁO: Mất tín hiệu với RC522 giữa chừng! (Ver: 0x%02X). Kiểm tra dây cắm!", current_ver);
            rgb_led_set_color(80, 0, 0);
        } else {
            ESP_LOGI(TAG, "⏳ [Heartbeat #%lu] RC522 đang hoạt động ổn định. Tổng số thẻ đã quẹt: %lu",
                     (unsigned long)loop_count, (unsigned long)s_tap_count);
        }
    }
}

#endif
