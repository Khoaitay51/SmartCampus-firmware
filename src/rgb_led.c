#include "rgb_led.h"
#include "app_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <string.h>
#include <math.h>

static const char *TAG = "RGB_LED";

static uint8_t s_target_r = 0;
static uint8_t s_target_g = 0;
static uint8_t s_target_b = 0;
static uint8_t s_brightness = 255;
static led_effect_t s_effect = LED_EFFECT_STATIC;
static SemaphoreHandle_t s_led_mutex = NULL;

static void apply_hardware_pwm(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t duty_r = (uint32_t)r;
    uint32_t duty_g = (uint32_t)g;
    uint32_t duty_b = (uint32_t)b;

#if RGB_LED_COMMON_ANODE
    duty_r = 255 - duty_r;
    duty_g = 255 - duty_g;
    duty_b = 255 - duty_b;
#endif

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_r);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_g);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty_b);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

static void rgb_led_task(void *pvParameters) {
    float phase = 0.0f;
    bool strobe_on = true;
    int strobe_count = 0;

    while (1) {
        xSemaphoreTake(s_led_mutex, portMAX_DELAY);
        led_effect_t eff = s_effect;
        uint8_t tr = s_target_r;
        uint8_t tg = s_target_g;
        uint8_t tb = s_target_b;
        uint8_t br = s_brightness;
        xSemaphoreGive(s_led_mutex);

        if (eff == LED_EFFECT_STATIC) {
            uint8_t r = (tr * br) / 255;
            uint8_t g = (tg * br) / 255;
            uint8_t b = (tb * br) / 255;
            apply_hardware_pwm(r, g, b);
            vTaskDelay(pdMS_TO_TICKS(50));
        } else if (eff == LED_EFFECT_BREATHE) {
            float factor = 0.15f + 0.85f * (0.5f * (1.0f + sinf(phase)));
            uint8_t cur_br = (uint8_t)(br * factor);
            apply_hardware_pwm((tr * cur_br) / 255, (tg * cur_br) / 255, (tb * cur_br) / 255);
            phase += 0.08f;
            if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
            vTaskDelay(pdMS_TO_TICKS(25));
        } else if (eff == LED_EFFECT_STROBE) {
            strobe_count++;
            if (strobe_count >= 4) {
                strobe_count = 0;
                strobe_on = !strobe_on;
            }
            if (strobe_on) {
                apply_hardware_pwm((tr * br) / 255, (tg * br) / 255, (tb * br) / 255);
            } else {
                apply_hardware_pwm(0, 0, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(25));
        }
    }
}

void rgb_led_init(void) {
    s_led_mutex = xSemaphoreCreateMutex();

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t chan_r = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = PIN_LED_RED,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&chan_r);

    ledc_channel_config_t chan_g = {
        .channel    = LEDC_CHANNEL_1,
        .duty       = 0,
        .gpio_num   = PIN_LED_GREEN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&chan_g);

    ledc_channel_config_t chan_b = {
        .channel    = LEDC_CHANNEL_2,
        .duty       = 0,
        .gpio_num   = PIN_LED_BLUE,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&chan_b);

    apply_hardware_pwm(0, 0, 0);
    xTaskCreate(rgb_led_task, "rgb_led_task", 2048, NULL, 4, NULL);
    ESP_LOGI(TAG, "RGB LED driver initialized (R:%d, G:%d, B:%d)", PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE);
}

void rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b) {
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    s_target_r = r;
    s_target_g = g;
    s_target_b = b;
    s_effect = LED_EFFECT_STATIC;
    s_brightness = 255;
    xSemaphoreGive(s_led_mutex);
}

void rgb_led_set_hex(const char *hex_color, led_effect_t effect, uint8_t brightness) {
    if (!hex_color) return;
    if (hex_color[0] == '#') hex_color++;
    unsigned int r = 0, g = 0, b = 0;
    if (sscanf(hex_color, "%02x%02x%02x", &r, &g, &b) == 3) {
        xSemaphoreTake(s_led_mutex, portMAX_DELAY);
        s_target_r = (uint8_t)r;
        s_target_g = (uint8_t)g;
        s_target_b = (uint8_t)b;
        s_effect = effect;
        s_brightness = brightness;
        xSemaphoreGive(s_led_mutex);
    }
}

void rgb_led_set_room_mode(const char *mode) {
    if (!mode) return;
    if (strcasecmp(mode, "saving") == 0) {
        rgb_led_set_hex("#000000", LED_EFFECT_STATIC, 255);
    } else if (strcasecmp(mode, "self_study") == 0 || strcasecmp(mode, "self-study") == 0) {
        rgb_led_set_hex("#66CCFF", LED_EFFECT_STATIC, 255);
    } else if (strcasecmp(mode, "lecture") == 0) {
        rgb_led_set_hex("#FFFFFF", LED_EFFECT_STATIC, 255);
    } else if (strcasecmp(mode, "exam") == 0) {
        rgb_led_set_hex("#FFBF00", LED_EFFECT_STATIC, 255);
    } else if (strcasecmp(mode, "lock") == 0) {
        rgb_led_set_hex("#808080", LED_EFFECT_STATIC, 120);
    } else if (strcasecmp(mode, "suspected") == 0) {
        rgb_led_set_hex("#FF8C00", LED_EFFECT_BREATHE, 255);
    } else if (strcasecmp(mode, "emergency") == 0) {
        rgb_led_set_hex("#FF0000", LED_EFFECT_STROBE, 255);
    }
}
