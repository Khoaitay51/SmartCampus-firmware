#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef void (*rfid_card_cb_t)(const char *card_uid);

void rc522_init(rfid_card_cb_t callback);
uint8_t rc522_get_version(void);
uint8_t rc522_get_raw_probe(uint8_t *raw_rx0, uint8_t *raw_rx1);
void rc522_pin_diagnostic(void);
void rc522_dump_registers(void);
