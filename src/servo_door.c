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
// Với Servo 360° / 180°:
// - SERVO_DOOR_INVERTED = 0 (Phòng 1 - Chiều tiêu chuẩn):
//     LOCKED: 820 ticks (~1.0ms, quay theo chiều đóng/khóa)
//     UNLOCKED: 1640 ticks (~2.0ms, quay theo chiều mở)
// - SERVO_DOOR_INVERTED = 1 (Phòng 2 - Đảo chiều chống đập cánh cửa do lắp đối xứng):
//     LOCKED: 1640 ticks (~2.0ms, quay theo chiều đóng/khóa)
//     UNLOCKED: 820 ticks (~1.0ms, quay theo chiều mở)
// - duty = 0: DỪNG HOÀN TOÀN (chống quay vô tận / chống quá nhiệt)
#if SERVO_DOOR_INVERTED
#define SERVO_DUTY_LOCKED   1640    // ~2.0ms (Chiều đóng đảo ngược cho Phòng 2)
#define SERVO_DUTY_UNLOCKED 820     // ~1.0ms (Chiều mở đảo ngược cho Phòng 2)
#else
#define SERVO_DUTY_LOCKED   820     // ~1.0ms (Chiều đóng chuẩn cho Phòng 1)
#define SERVO_DUTY_UNLOCKED 1640    // ~2.0ms (Chiều mở chuẩn cho Phòng 1)
#endif

#ifndef SERVO_RUN_TIME_MS
#define SERVO_RUN_TIME_MS   350     // Quay trong 350ms (~90 - 120 độ) rồi lập tức ngắt xung để dừng
#endif

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
    s_initialized = false; // Đặt false để lệnh đầu tiên sau khi boot luôn được thực thi dứt khoát

    ESP_LOGI(TAG, "Servo door initialized on GPIO %d (Inverted: %d, RunTime: %d ms, initial state: LOCKED)", 
             PIN_SERVO_DOOR, SERVO_DOOR_INVERTED, SERVO_RUN_TIME_MS);
}

void servo_door_set_locked(bool locked) {
    // Nếu trạng thái không thay đổi và đã từng kích hoạt, không kích hoạt quay lặp lại
    if (locked == s_is_locked && s_initialized) {
        return;
    }
    s_is_locked = locked;
    s_initialized = true;

    uint32_t duty = locked ? SERVO_DUTY_LOCKED : SERVO_DUTY_UNLOCKED;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3);
    ESP_LOGI(TAG, "Door state set to: %s (Duty: %lu, Inverted: %d, rotating for %d ms)", 
             locked ? "LOCKED" : "UNLOCKED", (unsigned long)duty, SERVO_DOOR_INVERTED, SERVO_RUN_TIME_MS);

    // Kích hoạt timer ngắt xung sau SERVO_RUN_TIME_MS để dừng servo dứt khoát
    if (s_servo_timer) {
        esp_timer_stop(s_servo_timer);
        esp_timer_start_once(s_servo_timer, (uint64_t)SERVO_RUN_TIME_MS * 1000);
    }
}

bool servo_door_is_locked(void) {
    return s_is_locked;
}
