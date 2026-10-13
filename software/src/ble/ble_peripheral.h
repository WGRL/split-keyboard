#ifndef BLE_PER_H
#define BLE_PER_H

#include "../bus/bus.h"
#include "../data_structures/bitset.h"
#include "ble_matrix_input.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

int ble_setup_slave();
void ble_task_slave(void *p1, void *p2, void *p3);

#endif
