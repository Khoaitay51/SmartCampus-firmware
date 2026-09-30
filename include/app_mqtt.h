#pragma once
#include <stdbool.h>

void app_mqtt_start(void);
bool app_mqtt_is_connected(void);
void app_mqtt_on_rfid_card_scanned(const char *card_uid);
