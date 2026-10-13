#ifndef USB_BLE_EVENT
#define USB_BLE_EVENT

#include "../bus/bus.h"
#include <stdbool.h>

#define USB_READY_BIT BIT(0)
#define USB_UNREADY_BIT BIT(1)

extern struct k_event ble_state_event;
extern struct k_event usb_state_event;

void usb_ready_changed(bool ready);

#endif
