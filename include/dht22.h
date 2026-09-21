#pragma once
#include "esp_err.h"

typedef struct {
    float temperature;
    float humidity;
} dht22_data_t;

esp_err_t dht22_init(int gpio_num);
esp_err_t dht22_read(dht22_data_t *data);
