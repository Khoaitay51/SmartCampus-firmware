#pragma once
#include <esp_err.h>
#include <stddef.h>

esp_err_t app_nvs_init(void);

// Room & Device ID
esp_err_t app_nvs_get_room_id(char *room_id_buf, size_t max_len);
esp_err_t app_nvs_set_room_id(const char *room_id);
esp_err_t app_nvs_get_device_id(char *device_id_buf, size_t max_len);
esp_err_t app_nvs_set_device_id(const char *device_id);

// Wi-Fi credentials (F1 fix: NVS override cho hardcoded defaults)
esp_err_t app_nvs_get_wifi_ssid(char *buf, size_t max_len);
esp_err_t app_nvs_set_wifi_ssid(const char *ssid);
esp_err_t app_nvs_get_wifi_pass(char *buf, size_t max_len);
esp_err_t app_nvs_set_wifi_pass(const char *pass);

// MQTT credentials (F1 fix: NVS override cho hardcoded defaults)
esp_err_t app_nvs_get_mqtt_uri(char *buf, size_t max_len);
esp_err_t app_nvs_set_mqtt_uri(const char *uri);
esp_err_t app_nvs_get_mqtt_pass(char *buf, size_t max_len);
esp_err_t app_nvs_set_mqtt_pass(const char *pass);
