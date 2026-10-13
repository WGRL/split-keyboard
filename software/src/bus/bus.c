#include "bus.h"

K_MSGQ_DEFINE(msgq_key, sizeof(struct pos), 30, 1);
K_MSGQ_DEFINE(msgq_kb_report, KB_REPORT_COUNT * sizeof(uint8_t), 10, 4);
K_MSGQ_DEFINE(msgq_peripheral_report, KB_REPORT_COUNT * sizeof(uint8_t), 10, 4);
K_MSGQ_DEFINE(msgq_key_codes, sizeof(struct key_code), 10, 4);

K_SEM_DEFINE(ble_init_sem, 0, 1);
K_SEM_DEFINE(usb_sem, 0, 1);

enum led_color led_state = black;
uint16_t pc_interval = USB_FREQ;

bool usb_ready = false;
int values_table_l[ROW_SIZE][COL_SIZE] = {
    {0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22}, // ESC, 1,2,3,4,5
    {0x2B, 0x14, 0x1A, 0x08, 0x15, 0x17}, // TAB, q w e r t
    {0x39, 0x04, 0x16, 0x07, 0x09, 0x0A}, // CAPS, a s d f g
    {0xE1, 0x1D, 0x1B, 0x06, 0x19, 0x05}, // SHIFT, z x c v b
    {0xE0, 0x2F, 0xE2, 0x2C, 0xE3, 0x35}  // CTRL, SUPER, L_ALT, SPACE, [, `
};

int values_table_r[ROW_SIZE][COL_SIZE] = {
    {0x2D, 0x27, 0x26, 0x25, 0x24, 0x23}, // - 0 9 8 7 6
    {0x2E, 0x13, 0x12, 0x0C, 0x18, 0x1C}, // = p o i u y
    {0x34, 0x33, 0x0F, 0x0E, 0x0D, 0x0B}, // ' ; l k j h
    {0x00, 0x38, 0x37, 0x36, 0x10, 0x11}, // X / . , m n
    {0x2A, 0x00, 0xE6, 0x31, 0x30, 0x28}  // backspace X R_ALT \ [ ]
};

void pos_reset(struct pos *button) {
  button->x = 255;
  button->y = 255;
  button->side = no_side;
}

bool usb_connected() { return nrf_power_usbregstatus_vbusdet_get(NRF_POWER); }
