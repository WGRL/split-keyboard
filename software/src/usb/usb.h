#ifndef USB_H
#define USB_H

#include "../ble/ble_hid.h"
#include "../bus/bus.h"
#include "../usb_ble_event/usb_ble_event.h"
#include "usb_init.h"
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/usb/usbd.h>

int create_usb(void);
int usb_send(void);
void usb_task(void *p1, void *p2, void *p3);

#endif
