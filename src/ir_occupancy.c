#include "ir_occupancy.h"
#include "app_config.h"
#include "buzzer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "IR_OCCUPANCY";
static occupancy_cb_t s_callback = NULL;
static ir_state_cb_t s_state_cb = NULL;

int ir_get_in_level(void) {
    return gpio_get_level(PIN_IR_IN);
}

int ir_get_out_level(void) {
    return gpio_get_level(PIN_IR_OUT);
}

static void ir_occupancy_task(void *pvParameters) {
    int last_in = gpio_get_level(PIN_IR_IN);
    int last_out = gpio_get_level(PIN_IR_OUT);
    ESP_LOGI(TAG, "IR Initial State: IN(GPIO%d)=%d, OUT(GPIO%d)=%d (1=Clear, 0=Blocked)", 
             PIN_IR_IN, last_in, PIN_IR_OUT, last_out);

    if (s_state_cb) {
        s_state_cb(last_in, last_out);
    }

    int first_triggered = 0; // 1: IN first, 2: OUT first
    int64_t trigger_time = 0;
    int64_t last_stuck_warning = 0;

    while (1) {
        int cur_in = gpio_get_level(PIN_IR_IN);
        int cur_out = gpio_get_level(PIN_IR_OUT);
        int64_t now = esp_timer_get_time() / 1000; // ms

        // Canh bao cam bien bi ket muc 0 (lien tuc sang den ma khong co vat can)
        if ((cur_in == 0 || cur_out == 0) && (now - last_stuck_warning > 5000)) {
            last_stuck_warning = now;
            if (cur_in == 0 && cur_out == 0) {
                ESP_LOGW(TAG, "Both IR IN(32) and OUT(33) are LOW (Blocked). Check sensitivity trimmers!");
            } else if (cur_in == 0) {
                ESP_LOGW(TAG, "IR IN (GPIO%d) is LOW (Blocked). If clear, turn blue trimmer counter-clockwise.", PIN_IR_IN);
            } else if (cur_out == 0) {
                ESP_LOGW(TAG, "IR OUT (GPIO%d) is LOW (Blocked). If clear, turn blue trimmer counter-clockwise.", PIN_IR_OUT);
            }
        }

        // Kiem tra su thay doi muc logic
        if (cur_in != last_in || cur_out != last_out) {
            if (cur_in != last_in) {
                ESP_LOGI(TAG, "Sensor IN (GPIO%d) -> %s (%d)", 
                         PIN_IR_IN, cur_in == 0 ? "DETECTED (Blocked)" : "CLEAR", cur_in);
            }
            if (cur_out != last_out) {
                ESP_LOGI(TAG, "Sensor OUT (GPIO%d) -> %s (%d)", 
                         PIN_IR_OUT, cur_out == 0 ? "DETECTED (Blocked)" : "CLEAR", cur_out);
            }

            // Cap nhat man hinh OLED ngay khi trang thai thay doi
            if (s_state_cb) {
                s_state_cb(cur_in, cur_out);
            }
        }

        // Xu ly chuoi di chuyen:
        // Tranh false trigger khi ca 2 sensor bi che cung luc
        bool in_just_blocked  = (cur_in == 0 && last_in == 1);
        bool out_just_blocked = (cur_out == 0 && last_out == 1);
        if (in_just_blocked && out_just_blocked) {
            ESP_LOGW(TAG, "Both sensors triggered simultaneously, ignoring.");
            in_just_blocked = false;
            out_just_blocked = false;
        }

        // Cam bien IN vua bi che (1 -> 0)
        if (in_just_blocked) {
            buzzer_play(BUZZER_PATTERN_SHORT);
            if (first_triggered == 2 && (now - trigger_time < 3000)) {
                ESP_LOGI(TAG, ">>> SEQUENCE COMPLETED: OUT -> IN (Person EXITED room)");
                if (s_callback) s_callback(OCCUPANCY_OUT);
                first_triggered = 0;
            } else if (first_triggered == 0) {
                first_triggered = 1;
                trigger_time = now;
                ESP_LOGI(TAG, "Step 1/2: IN triggered. Waiting for OUT within 3.0s...");
            }
        }

        // Cam bien OUT vua bi che (1 -> 0)
        if (out_just_blocked) {
            if (first_triggered == 1 && (now - trigger_time < 3000)) {
                // Step 2 hoan thanh: khong can buzzer SHORT vi callback se play DOUBLE
                ESP_LOGI(TAG, ">>> SEQUENCE COMPLETED: IN -> OUT (Person ENTERED room)");
                if (s_callback) s_callback(OCCUPANCY_IN);
                first_triggered = 0;
            } else if (first_triggered == 0) {
                buzzer_play(BUZZER_PATTERN_SHORT);
                first_triggered = 2;
                trigger_time = now;
                ESP_LOGI(TAG, "Step 1/2: OUT triggered. Waiting for IN within 3.0s...");
            }
        }

        // Timeout 3.0s neu khong hoan thanh chuoi
        if (first_triggered != 0 && (now - trigger_time > 3000)) {
            ESP_LOGW(TAG, "Sequence timeout (no second sensor triggered). Resetting.");
            first_triggered = 0;
        }

        last_in = cur_in;
        last_out = cur_out;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void ir_occupancy_init(occupancy_cb_t callback, ir_state_cb_t state_cb) {
    s_callback = callback;
    s_state_cb = state_cb;

    gpio_reset_pin(PIN_IR_IN);
    gpio_set_direction(PIN_IR_IN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_IR_IN, GPIO_PULLUP_ONLY);

    gpio_reset_pin(PIN_IR_OUT);
    gpio_set_direction(PIN_IR_OUT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_IR_OUT, GPIO_PULLUP_ONLY);

    xTaskCreate(ir_occupancy_task, "ir_occupancy_task", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "IR Occupancy initialized on GPIO %d (IN) and GPIO %d (OUT)", PIN_IR_IN, PIN_IR_OUT);
}
