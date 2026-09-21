#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef void (*rfid_card_cb_t)(const char *card_uid);

void rc522_init(rfid_card_cb_t callback);
