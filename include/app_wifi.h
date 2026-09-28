#pragma once
#include <esp_err.h>

#include <stdbool.h>

void app_wifi_init_sta(void);
bool app_wifi_wait_connected(uint32_t timeout_ms);
bool app_wifi_is_connected(void);
void app_wifi_get_mac_str(char *mac_str);
