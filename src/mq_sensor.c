#include "mq_sensor.h"
#include "app_config.h"
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

static const char *TAG = "MQ_SENSOR";

#define SMOKE_THRESHOLD_SUSPECTED  1400.0f
#define SMOKE_THRESHOLD_EMERGENCY  2400.0f

static adc_oneshot_unit_handle_t s_adc_handle = NULL;

void mq_sensor_init(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC1 unit: %s", esp_err_to_name(ret));
        return;
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(s_adc_handle, ADC_CHANNEL_6, &chan_config); // GPIO 34 (MQ-2)
    adc_oneshot_config_channel(s_adc_handle, ADC_CHANNEL_7, &chan_config); // GPIO 35 (MQ-135)

    ESP_LOGI(TAG, "MQ-2 on GPIO %d (ADC1_CH6), MQ-135 on GPIO %d (ADC1_CH7) initialized", PIN_MQ2_ADC, PIN_MQ135_ADC);
}

void mq_sensor_read(mq_data_t *data) {
    if (!data) return;

    int raw_mq2 = 0;
    int raw_mq135 = 0;

    if (s_adc_handle != NULL) {
        for (int i = 0; i < 8; i++) {
            int val = 0;
            adc_oneshot_read(s_adc_handle, ADC_CHANNEL_6, &val);
            raw_mq2 += val;
            adc_oneshot_read(s_adc_handle, ADC_CHANNEL_7, &val);
            raw_mq135 += val;
        }
        raw_mq2 /= 8;
        raw_mq135 /= 8;
    }

    data->smoke_raw = (float)raw_mq2;

    if (data->smoke_raw >= SMOKE_THRESHOLD_EMERGENCY) {
        data->smoke_detected = true;
        data->smoke_state = "emergency";
    } else if (data->smoke_raw >= SMOKE_THRESHOLD_SUSPECTED) {
        data->smoke_detected = true;
        data->smoke_state = "suspected";
    } else {
        data->smoke_detected = false;
        data->smoke_state = "normal";
    }

    // Uoc luong CO2 va Air Quality tu MQ-135
    data->co2_ppm = 400 + (raw_mq135 * 1600) / 4095;
    data->air_quality = 30 + (raw_mq135 * 170) / 4095;
}
