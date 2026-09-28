#include "fan_control.h"
#include "app_config.h"
#include <driver/ledc.h>
#include <esp_log.h>

static const char *TAG = "FAN_CONTROL";
static bool s_is_on = false;
static uint8_t s_speed_pct = 0;

void fan_control_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_2,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err_timer = ledc_timer_config(&ledc_timer);
    ESP_LOGI(TAG, "LEDC Timer 2 config: %s", esp_err_to_name(err_timer));

    uint32_t init_duty = 0;
#if !FAN_ACTIVE_HIGH
    init_duty = 255;
#endif

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_4,
        .duty       = init_duty,
        .gpio_num   = PIN_FAN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_2
    };
    esp_err_t err_chan = ledc_channel_config(&ledc_channel);
    ESP_LOGI(TAG, "Fan initialized on GPIO %d (Status: OFF, ret: %s)", PIN_FAN, esp_err_to_name(err_chan));

    s_is_on = false;
    s_speed_pct = 0;
}

void fan_control_set_speed(uint8_t speed_pct) {
    if (speed_pct > 100) speed_pct = 100;
    s_speed_pct = speed_pct;
    s_is_on = (speed_pct > 0);

    uint32_t duty = (speed_pct * 255) / 100;
#if !FAN_ACTIVE_HIGH
    duty = 255 - duty;
#endif

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
    ESP_LOGI(TAG, "Fan speed set to: %d%% (Duty: %lu/255)", speed_pct, (unsigned long)duty);
}

void fan_control_set_state(bool on) {
    fan_control_set_speed(on ? 100 : 0);
}

bool fan_control_is_on(void) {
    return s_is_on;
}

uint8_t fan_control_get_speed(void) {
    return s_speed_pct;
}
