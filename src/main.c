#include <stdio.h>
#include <esp_log.h>
#include "app_config.h"
#include "app_nvs.h"
#include "app_wifi.h"
#include "app_sntp.h"
#include "rgb_led.h"
#include "dht22.h"
#include "mq_sensor.h"
#include "servo_door.h"
#include "buzzer.h"
#include "oled_ssd1306.h"
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

#if CURRENT_NODE_ROLE != ROLE_OLED_DISPLAY_NODE && CURRENT_NODE_ROLE != ROLE_CORRIDOR_NODE
    // 3. Khoi tao Sensors (Chi can tren Sensor / IR hoac Full Node)
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

