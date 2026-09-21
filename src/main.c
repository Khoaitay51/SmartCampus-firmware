#include <stdio.h>
#include <esp_log.h>
#include "app_config.h"
#include "app_nvs.h"
#include "app_wifi.h"
#include "rgb_led.h"
#include "dht22.h"
#include "mq_sensor.h"
#include "servo_door.h"
#include "buzzer.h"
#include "oled_ssd1306.h"
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
    oled_init();

    // 3. Khoi tao Sensors
    dht22_init(PIN_DHT22);
    mq_sensor_init();

    // 4. Ket noi Wi-Fi
    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    app_wifi_init_sta();
    app_wifi_wait_connected();
    ESP_LOGI(TAG, "Wi-Fi connected successfully!");

    // 5. Khoi dong MQTT va cac Peripheral tasks (RFID, IR, Telemetry)
    app_mqtt_start();

    ESP_LOGI(TAG, "=== SYSTEM ALL-IN-ONE OPERATIONAL ===");
}
