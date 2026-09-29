#include "app_nvs.h"
#include "app_config.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "APP_NVS";

esp_err_t app_nvs_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

// ---------------------------------------------------------------------------
// Generic NVS string get/set helpers
// ---------------------------------------------------------------------------

static esp_err_t nvs_get_string(const char *key, char *buf, size_t max_len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_str(handle, key, buf, &max_len);
    nvs_close(handle);
    return err;
}

static esp_err_t nvs_set_string(const char *key, const char *value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Saved NVS key '%s'", key);
    return err;
}

// ---------------------------------------------------------------------------
// Room & Device ID
// ---------------------------------------------------------------------------

esp_err_t app_nvs_get_room_id(char *room_id_buf, size_t max_len) {
    return nvs_get_string(NVS_KEY_ROOM_ID, room_id_buf, max_len);
}

esp_err_t app_nvs_set_room_id(const char *room_id) {
    return nvs_set_string(NVS_KEY_ROOM_ID, room_id);
}

esp_err_t app_nvs_get_device_id(char *device_id_buf, size_t max_len) {
    return nvs_get_string(NVS_KEY_DEVICE_ID, device_id_buf, max_len);
}

esp_err_t app_nvs_set_device_id(const char *device_id) {
    return nvs_set_string(NVS_KEY_DEVICE_ID, device_id);
}

// ---------------------------------------------------------------------------
// Wi-Fi credentials (F1 fix)
// ---------------------------------------------------------------------------

esp_err_t app_nvs_get_wifi_ssid(char *buf, size_t max_len) {
    esp_err_t err = nvs_get_string(NVS_KEY_WIFI_SSID, buf, max_len);
    if (err != ESP_OK) {
        // Fallback ve gia tri mac dinh khi NVS chua co
        strncpy(buf, DEFAULT_WIFI_SSID, max_len - 1);
        buf[max_len - 1] = '\0';
        ESP_LOGI(TAG, "WiFi SSID not in NVS, using default: %s", buf);
    }
    return ESP_OK; // Luon tra ve OK vi co fallback
}

esp_err_t app_nvs_set_wifi_ssid(const char *ssid) {
    return nvs_set_string(NVS_KEY_WIFI_SSID, ssid);
}

esp_err_t app_nvs_get_wifi_pass(char *buf, size_t max_len) {
    esp_err_t err = nvs_get_string(NVS_KEY_WIFI_PASS, buf, max_len);
    if (err != ESP_OK) {
        strncpy(buf, DEFAULT_WIFI_PASSWORD, max_len - 1);
        buf[max_len - 1] = '\0';
        ESP_LOGI(TAG, "WiFi password not in NVS, using default");
    }
    return ESP_OK;
}

esp_err_t app_nvs_set_wifi_pass(const char *pass) {
    return nvs_set_string(NVS_KEY_WIFI_PASS, pass);
}

// ---------------------------------------------------------------------------
// MQTT credentials (F1 fix)
// ---------------------------------------------------------------------------

esp_err_t app_nvs_get_mqtt_uri(char *buf, size_t max_len) {
    esp_err_t err = nvs_get_string(NVS_KEY_MQTT_URI, buf, max_len);
    if (err != ESP_OK) {
        strncpy(buf, DEFAULT_MQTT_BROKER_URI, max_len - 1);
        buf[max_len - 1] = '\0';
        ESP_LOGI(TAG, "MQTT URI not in NVS, using default: %s", buf);
    }
    return ESP_OK;
}

esp_err_t app_nvs_set_mqtt_uri(const char *uri) {
    return nvs_set_string(NVS_KEY_MQTT_URI, uri);
}

esp_err_t app_nvs_get_mqtt_pass(char *buf, size_t max_len) {
    esp_err_t err = nvs_get_string(NVS_KEY_MQTT_PASS, buf, max_len);
    if (err != ESP_OK) {
        strncpy(buf, DEFAULT_MQTT_PASSWORD, max_len - 1);
        buf[max_len - 1] = '\0';
        ESP_LOGI(TAG, "MQTT password not in NVS, using default");
    }
    return ESP_OK;
}

esp_err_t app_nvs_set_mqtt_pass(const char *pass) {
    return nvs_set_string(NVS_KEY_MQTT_PASS, pass);
}
