#include "ir_occupancy.h"
#include "app_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "IR_OCCUPANCY";
static occupancy_cb_t s_callback = NULL;

static void ir_occupancy_task(void *pvParameters) {
    int last_ir_in = 1;
    int last_ir_out = 1;
    int first_triggered = 0; // 1: IN triggered first, 2: OUT triggered first
    int64_t trigger_time = 0;

    while (1) {
        int cur_in = gpio_get_level(PIN_IR_IN);
        int cur_out = gpio_get_level(PIN_IR_OUT);
        int64_t now = esp_timer_get_time() / 1000; // ms

        // Cảm biến vật cản hồng ngoại: Mức LOW (0) là có người che
        if (cur_in == 0 && last_ir_in == 1) {
            if (first_triggered == 2 && (now - trigger_time < 1500)) {
                ESP_LOGI(TAG, "Person exited room (OUT)");
                if (s_callback) s_callback(OCCUPANCY_OUT);
                first_triggered = 0;
            } else {
                first_triggered = 1;
                trigger_time = now;
            }
        }

        if (cur_out == 0 && last_ir_out == 1) {
            if (first_triggered == 1 && (now - trigger_time < 1500)) {
                ESP_LOGI(TAG, "Person entered room (IN)");
                if (s_callback) s_callback(OCCUPANCY_IN);
                first_triggered = 0;
            } else {
                first_triggered = 2;
                trigger_time = now;
            }
        }

        // Timeout sau 1.8s nếu không hoàn thành chu kỳ bước qua cả 2 tia
        if (first_triggered != 0 && (now - trigger_time > 1800)) {
            first_triggered = 0;
        }

        last_ir_in = cur_in;
        last_ir_out = cur_out;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void ir_occupancy_init(occupancy_cb_t callback) {
    s_callback = callback;

    gpio_reset_pin(PIN_IR_IN);
    gpio_set_direction(PIN_IR_IN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_IR_IN, GPIO_PULLUP_ONLY);

    gpio_reset_pin(PIN_IR_OUT);
    gpio_set_direction(PIN_IR_OUT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_IR_OUT, GPIO_PULLUP_ONLY);

    xTaskCreate(ir_occupancy_task, "ir_occupancy_task", 2048, NULL, 6, NULL);
    ESP_LOGI(TAG, "IR Occupancy initialized on GPIO %d (IN) & GPIO %d (OUT)", PIN_IR_IN, PIN_IR_OUT);
}
