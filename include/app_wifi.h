#pragma once
#include <esp_err.h>

void app_wifi_init_sta(void);
void app_wifi_wait_connected(void);
void app_wifi_get_mac_str(char *mac_str);
