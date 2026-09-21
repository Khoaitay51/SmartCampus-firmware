#pragma once
#include <esp_err.h>

esp_err_t app_nvs_init(void);
esp_err_t app_nvs_get_room_id(char *room_id_buf, size_t max_len);
esp_err_t app_nvs_set_room_id(const char *room_id);
esp_err_t app_nvs_get_device_id(char *device_id_buf, size_t max_len);
esp_err_t app_nvs_set_device_id(const char *device_id);
