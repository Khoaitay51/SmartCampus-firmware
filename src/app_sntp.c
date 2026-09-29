#include "app_sntp.h"
#include <esp_log.h>
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "APP_SNTP";
static volatile bool s_time_synced = false;

/**
 * Callback khi SNTP đồng bộ thành công.
 * Được gọi bởi lwIP SNTP stack sau mỗi lần cập nhật thời gian.
 */
static void sntp_sync_notification_cb(struct timeval *tv) {
    s_time_synced = true;

    time_t now = tv->tv_sec;
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    char strftime_buf[30];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    ESP_LOGI(TAG, "SNTP time synchronized: %s", strftime_buf);
}

void app_sntp_init(void) {
    ESP_LOGI(TAG, "Initializing SNTP (server: pool.ntp.org)...");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    sntp_set_time_sync_notification_cb(sntp_sync_notification_cb);

    // Cho phep SNTP tu dong cap nhat moi 15 phut (mac dinh)
    esp_sntp_init();
}

bool app_sntp_wait_synced(uint32_t timeout_ms) {
    if (s_time_synced) return true;

    ESP_LOGI(TAG, "Waiting for SNTP sync (timeout: %lu ms)...", (unsigned long)timeout_ms);

    uint32_t elapsed = 0;
    const uint32_t poll_ms = 100;

    while (!s_time_synced && elapsed < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(poll_ms));
        elapsed += poll_ms;
    }

    if (s_time_synced) {
        ESP_LOGI(TAG, "SNTP synced after ~%lu ms", (unsigned long)elapsed);
    } else {
        ESP_LOGW(TAG, "SNTP sync timeout after %lu ms. Timestamps will use uptime estimate.", (unsigned long)timeout_ms);
    }
    return s_time_synced;
}

bool app_sntp_is_synced(void) {
    return s_time_synced;
}

void app_sntp_get_iso8601(char *buf, size_t buf_len) {
    if (!buf || buf_len < 25) {
        if (buf && buf_len > 0) buf[0] = '\0';
        return;
    }

    time_t now;
    time(&now);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    // Kiem tra thoi gian co hop le (truoc 2024 -> chua sync)
    if (timeinfo.tm_year < (2024 - 1900)) {
        // Chua dong bo, dung uptime gia lap tu epoch 2026-01-01T00:00:00Z
        snprintf(buf, buf_len, "1970-01-01T00:00:00Z");
        return;
    }

    strftime(buf, buf_len, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}
