#include "usb_ble_event.h"

K_EVENT_DEFINE(usb_state_event);
K_EVENT_DEFINE(ble_state_event);

void usb_ready_changed(bool ready) {
  usb_ready = ready;
  uint32_t bit = ready ? USB_READY_BIT : USB_UNREADY_BIT;
  k_event_post(&ble_state_event, bit);
  k_event_post(&usb_state_event, bit);
}
