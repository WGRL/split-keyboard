#ifndef HOG_H
#define HOG_H

#include "../bus/bus.h"
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/bluetooth/conn.h>

/* HID over GATT (HOGP) keyboard service exposed to the PC. */

/* True when the PC has enabled notifications on the input report. */
bool hog_is_subscribed(struct bt_conn *conn);

/* Send an 8-byte boot-style keyboard report (mod, reserved, 6 keys). */
int hog_send_report(struct bt_conn *conn, const uint8_t report[KB_REPORT_COUNT]);

/* Last LED state written by the PC (bit0 NumLock, bit1 CapsLock, ...). */
uint8_t hog_get_leds(void);

#endif
