#include "buzzer.h"
#include "app_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <stdatomic.h>

static const char *TAG = "BUZZER";
static _Atomic buzzer_pattern_t s_current_pattern = BUZZER_PATTERN_OFF;

static void buzzer_on(void) {
#if BUZZER_ACTIVE_LOW
    gpio_set_level(PIN_BUZZER, 0);
#else
    gpio_set_level(PIN_BUZZER, 1);
#endif
}

static void buzzer_off(void) {
#if BUZZER_ACTIVE_LOW
    gpio_set_level(PIN_BUZZER, 1);
#else
    gpio_set_level(PIN_BUZZER, 0);
#endif
}

static void buzzer_task(void *pvParameters) {
    while (1) {
        switch (s_current_pattern) {
            case BUZZER_PATTERN_SHORT:
                buzzer_on();
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_off();
                s_current_pattern = BUZZER_PATTERN_OFF;
                break;

            case BUZZER_PATTERN_LONG:
                buzzer_on();
                vTaskDelay(pdMS_TO_TICKS(800));
                buzzer_off();
                s_current_pattern = BUZZER_PATTERN_OFF;
                break;

            case BUZZER_PATTERN_DOUBLE:
                buzzer_on();
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_off();
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_on();
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_off();
                s_current_pattern = BUZZER_PATTERN_OFF;
                break;

            case BUZZER_PATTERN_EMERGENCY:
                buzzer_on();
                vTaskDelay(pdMS_TO_TICKS(200));
                buzzer_off();
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case BUZZER_PATTERN_OFF:
            default:
                buzzer_off();
                vTaskDelay(pdMS_TO_TICKS(50));
                break;
        }
    }
}

void buzzer_init(void) {
    gpio_reset_pin(PIN_BUZZER);
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_OUTPUT);
    buzzer_off();
    xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", PIN_BUZZER);
}

void buzzer_play(buzzer_pattern_t pattern) {
    s_current_pattern = pattern;
}

void buzzer_set_state(bool on) {
    if (on) s_current_pattern = BUZZER_PATTERN_SHORT;
    else s_current_pattern = BUZZER_PATTERN_OFF;
}
