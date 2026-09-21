#include "app_nvs.h"
#include "app_config.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>

static const char *TAG = "APP_NVS";

esp_err_t app_nvs_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t app_nvs_get_room_id(char *room_id_buf, size_t max_len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_str(handle, NVS_KEY_ROOM_ID, room_id_buf, &max_len);
    nvs_close(handle);
    return err;
}

esp_err_t app_nvs_set_room_id(const char *room_id) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, NVS_KEY_ROOM_ID, room_id);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Saved room_id to NVS: %s", room_id);
    return err;
}

esp_err_t app_nvs_get_device_id(char *device_id_buf, size_t max_len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_str(handle, NVS_KEY_DEVICE_ID, device_id_buf, &max_len);
    nvs_close(handle);
    return err;
}

esp_err_t app_nvs_set_device_id(const char *device_id) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, NVS_KEY_DEVICE_ID, device_id);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
