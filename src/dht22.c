#include "dht22.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <esp_log.h>

static const char *TAG = "DHT22";
static int s_gpio_num = -1;

esp_err_t dht22_init(int gpio_num) {
    s_gpio_num = gpio_num;
    gpio_reset_pin(s_gpio_num);
    gpio_set_direction(s_gpio_num, GPIO_MODE_INPUT);
    gpio_set_pull_mode(s_gpio_num, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "DHT22 initialized on GPIO %d", gpio_num);
    return ESP_OK;
}

static int wait_pulse_level(int level, uint32_t timeout_us) {
    uint32_t start = esp_timer_get_time();
    while (gpio_get_level(s_gpio_num) == level) {
        if ((uint32_t)(esp_timer_get_time() - start) > timeout_us) {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start);
}

esp_err_t dht22_read(dht22_data_t *data) {
    if (s_gpio_num < 0 || !data) return ESP_ERR_INVALID_ARG;

    uint8_t bytes[5] = {0};

    // 1. Gui tin hieu Start: Keo LOW >= 18ms
    gpio_set_direction(s_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(s_gpio_num, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 2. Keo HIGH 30us va chuyen sang INPUT
    gpio_set_level(s_gpio_num, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(s_gpio_num, GPIO_MODE_INPUT);

    // 3. Khoa gian doan de do thoi gian chinh xac (~4ms)
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    // Cho DHT phan hoi (LOW 80us, HIGH 80us)
    if (wait_pulse_level(1, 100) < 0) { portEXIT_CRITICAL(&mux); return ESP_ERR_TIMEOUT; }
    if (wait_pulse_level(0, 100) < 0) { portEXIT_CRITICAL(&mux); return ESP_ERR_TIMEOUT; }
    if (wait_pulse_level(1, 100) < 0) { portEXIT_CRITICAL(&mux); return ESP_ERR_TIMEOUT; }

    // 4. Doc 40 bits (5 bytes)
    for (int i = 0; i < 40; i++) {
        if (wait_pulse_level(0, 70) < 0) {
            portEXIT_CRITICAL(&mux);
            return ESP_ERR_TIMEOUT;
        }
        int high_us = wait_pulse_level(1, 90);
        if (high_us < 0) {
            portEXIT_CRITICAL(&mux);
            return ESP_ERR_TIMEOUT;
        }

        if (high_us > 40) {
            bytes[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    portEXIT_CRITICAL(&mux);

    // 5. Kiem tra Checksum
    uint8_t checksum = (bytes[0] + bytes[1] + bytes[2] + bytes[3]) & 0xFF;
    if (checksum != bytes[4]) {
        ESP_LOGW(TAG, "Checksum error: calc=0x%02X, recv=0x%02X", checksum, bytes[4]);
        return ESP_ERR_INVALID_CRC;
    }

    // 6. Tinh toan gia tri nhiet do & do am
    uint16_t raw_hum = ((uint16_t)bytes[0] << 8) | bytes[1];
    uint16_t raw_temp = (((uint16_t)bytes[2] & 0x7F) << 8) | bytes[3];

    data->humidity = raw_hum / 10.0f;
    data->temperature = raw_temp / 10.0f;
    if (bytes[2] & 0x80) {
        data->temperature = -data->temperature;
    }

    return ESP_OK;
}
