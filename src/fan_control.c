#include "fan_control.h"
#include "app_config.h"
#include <esp_log.h>

#if !FAN_2WIRE_GPIO
#include <driver/ledc.h>
#endif

#include <driver/gpio.h>

static const char *TAG = "FAN_CONTROL";
static bool s_is_on = false;
static uint8_t s_speed_pct = 0;

// ============================================================================
// Che do 1: Quat 2 day — GPIO on/off (FAN_2WIRE_GPIO = 1)
//
// Quat DC 5V chi co 2 day (VCC + GND), khong co chan PWM.
// Dieu khien bang cach bat/tat nguon qua Transistor NPN hoac MOSFET N-channel:
//
//   GPIO14 ---[1kΩ]--- Base(NPN) --- Collector --- GND_Fan
//                       |                           |
//                      GND                        VCC 5V
//
// FAN_ACTIVE_HIGH = 1: GPIO HIGH = bat quat (NPN / N-MOSFET)
// FAN_ACTIVE_HIGH = 0: GPIO LOW  = bat quat (Relay Active LOW)
// ============================================================================

#if FAN_2WIRE_GPIO

void fan_control_init(void) {
    gpio_reset_pin(PIN_FAN);
    gpio_set_direction(PIN_FAN, GPIO_MODE_OUTPUT);

    // Tat quat khi khoi dong
#if FAN_ACTIVE_HIGH
    gpio_set_level(PIN_FAN, 0);
#else
    gpio_set_level(PIN_FAN, 1);
#endif

    s_is_on = false;
    s_speed_pct = 0;
    ESP_LOGI(TAG, "Fan (2-wire GPIO) initialized on GPIO %d (OFF)", PIN_FAN);
}

void fan_control_set_state(bool on) {
    s_is_on = on;
    s_speed_pct = on ? 100 : 0;

#if FAN_ACTIVE_HIGH
    gpio_set_level(PIN_FAN, on ? 1 : 0);
#else
    gpio_set_level(PIN_FAN, on ? 0 : 1);
#endif

    ESP_LOGI(TAG, "Fan %s (GPIO %d = %d)",
             on ? "ON" : "OFF", PIN_FAN, gpio_get_level(PIN_FAN));
}

void fan_control_set_speed(uint8_t speed_pct) {
    // Quat 2 day khong co speed control -> map thanh on/off
    if (speed_pct > 100) speed_pct = 100;
    fan_control_set_state(speed_pct > 0);
}

bool fan_control_is_on(void) {
    return s_is_on;
}

uint8_t fan_control_get_speed(void) {
    return s_speed_pct;
}

// ============================================================================
// Che do 2: Quat 3-4 day — PWM speed control via LEDC (FAN_2WIRE_GPIO = 0)
// ============================================================================

#else  // PWM mode

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
    ESP_LOGI(TAG, "Fan (PWM) initialized on GPIO %d (OFF, ret: %s)", PIN_FAN, esp_err_to_name(err_chan));

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

#endif  // FAN_2WIRE_GPIO
