#ifndef BLE_MATRIX_INPUT_H
#define BLE_MATRIX_INPUT_H

#include "../bus/bus.h"
#include <hal/nrf_power.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "../data_structures/bitset.h"
#include <string.h>

void create_report(uint8_t report[KB_REPORT_COUNT]);
size_t get_all_queue_l(struct key_code *keys);
#endif
