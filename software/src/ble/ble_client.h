#ifndef BLE_CLIENT_H
#define BLE_CLIENT_H

#include "../bus/bus.h"
#include <hal/nrf_power.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#define CLI_FREQ 30
#define DEVICE_NAME "XIAO_BLE_R"

void ble_task_master(void *p1, void *p2, void *p3);

int ble_setup_master();

#endif
