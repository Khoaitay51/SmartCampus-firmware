#include "servo_door.h"
#include "app_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "SERVO_DOOR";
static bool s_is_locked = true;
static bool s_initialized = false;
static esp_timer_handle_t s_servo_timer = NULL;

// Cấu hình cho Servo SG90 (Hỗ trợ hoàn hảo cả Servo 180° và Servo 360° quay liên tục):
// Với Servo 360°:
// - 1.0ms (~820 ticks): Quay theo chiều đóng/khóa
// - 2.0ms (~1640 ticks): Quay theo chiều mở
// - duty = 0: DỪNG HOÀN TOÀN (chống quay vô tận)
#define SERVO_DUTY_LOCKED   820     // ~1.0ms (Chiều đóng)
#define SERVO_DUTY_UNLOCKED 1640    // ~2.0ms (Chiều mở)
#define SERVO_RUN_TIME_MS   350     // Quay trong 350ms (~90 - 120 độ) rồi lập tức ngắt xung để dừng

static void servo_stop_timer_cb(void *arg) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3);
    ESP_LOGI(TAG, "Servo stopped completely (duty = 0)");
}

void servo_door_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_1,
        .duty_resolution  = LEDC_TIMER_14_BIT,
        .freq_hz          = 50,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err_timer = ledc_timer_config(&ledc_timer);
    ESP_LOGI(TAG, "LEDC Timer 1 config: %s (Freq: %lu Hz)", 
             esp_err_to_name(err_timer), (unsigned long)ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1));

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_3,
        .duty       = 0, // Khởi tạo ban đầu = 0 để đứng yên, không quay loạn khi boot
        .gpio_num   = PIN_SERVO_DOOR,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_1
    };
    esp_err_t err_chan = ledc_channel_config(&ledc_channel);
    ESP_LOGI(TAG, "LEDC Channel 3 config: %s on GPIO %d", esp_err_to_name(err_chan), PIN_SERVO_DOOR);

    const esp_timer_create_args_t timer_args = {
        .callback = &servo_stop_timer_cb,
        .name = "servo_stop_timer"
    };
    esp_timer_create(&timer_args, &s_servo_timer);

    s_is_locked = true;
    s_initialized = true;

    ESP_LOGI(TAG, "Servo door initialized on GPIO %d (Anti-continuous rotation enabled, initial state: LOCKED)", PIN_SERVO_DOOR);
}

void servo_door_set_locked(bool locked) {
    // Nếu trạng thái không thay đổi, không kích hoạt quay lặp lại
    if (locked == s_is_locked && s_initialized) {
        return;
    }
    s_is_locked = locked;
    s_initialized = true;

    uint32_t duty = locked ? SERVO_DUTY_LOCKED : SERVO_DUTY_UNLOCKED;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3);
    ESP_LOGI(TAG, "Door state set to: %s (Duty: %lu, rotating for %d ms)", 
             locked ? "LOCKED" : "UNLOCKED", (unsigned long)duty, SERVO_RUN_TIME_MS);

    // Kích hoạt timer ngắt xung sau SERVO_RUN_TIME_MS để dừng servo dứt khoát
    if (s_servo_timer) {
        esp_timer_stop(s_servo_timer);
        esp_timer_start_once(s_servo_timer, SERVO_RUN_TIME_MS * 1000);
    }
}

bool servo_door_is_locked(void) {
    return s_is_locked;
}
