#ifndef BLE_HID_H
#define BLE_HID_H

#include "../bus/bus.h"
#include "../usb_ble_event/usb_ble_event.h"
#include "hog.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

void ble_pc_task(void *p1, void *p2, void *p3);
void usb_ready_changed(bool ready);

#endif
