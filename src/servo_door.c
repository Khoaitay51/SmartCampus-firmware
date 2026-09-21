#include "servo_door.h"
#include "app_config.h"
#include <driver/ledc.h>
#include <esp_log.h>

static const char *TAG = "SERVO_DOOR";
static bool s_is_locked = true;

// Servo SG90: 50Hz (20ms chu kỳ). Độ rộng xung: 1.0ms (0 độ - Đóng/Khóa) đến 2.0ms (90/180 độ - Mở)
// 14-bit resolution: 20ms = 16384 ticks -> 1ms = 819 ticks, 2ms = 1638 ticks
#define SERVO_DUTY_LOCKED   819     // 1.0ms (~0 độ)
#define SERVO_DUTY_UNLOCKED 1638    // 2.0ms (~90/180 độ)

void servo_door_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_1,
        .duty_resolution  = LEDC_TIMER_14_BIT,
        .freq_hz          = 50,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_3,
        .duty       = SERVO_DUTY_LOCKED,
        .gpio_num   = PIN_SERVO_DOOR,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_1
    };
    ledc_channel_config(&ledc_channel);
    servo_door_set_locked(true);
    ESP_LOGI(TAG, "Servo door initialized on GPIO %d (Status: LOCKED)", PIN_SERVO_DOOR);
}

void servo_door_set_locked(bool locked) {
    s_is_locked = locked;
    uint32_t duty = locked ? SERVO_DUTY_LOCKED : SERVO_DUTY_UNLOCKED;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3);
    ESP_LOGI(TAG, "Door state set to: %s", locked ? "LOCKED" : "UNLOCKED");
}

bool servo_door_is_locked(void) {
    return s_is_locked;
}
