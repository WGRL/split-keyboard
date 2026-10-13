#ifndef BUS_H
#define BUS_H

#include <hal/nrf_power.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#define ROW_SIZE 5
#define COL_SIZE 6

#define HID_LIM 6
#define KB_REPORT_LEN 9
#define XY_LIMIT 200

#define MATRIX_FREQ 5
#define USB_FREQ 15
#define RET_FINE 0
#define INTERVAL_MUL 125 / 100

extern int values_table_l[5][6];
extern int values_table_r[5][6];

enum side { left, right, no_side };
enum state { pressed, released };
enum led_color { black, white, red, green, blue };

// structures
struct pos {
  enum state state;
  enum side side;
  uint8_t x;
  uint8_t y;
  uint8_t modifier;
};

struct key_code {
  uint8_t value;
  uint8_t modifier;
};

enum kb_report_idx {
  KB_MOD_KEY = 0,
  KB_RESERVED,
  KB_KEY_CODE1,
  KB_KEY_CODE2,
  KB_KEY_CODE3,
  KB_KEY_CODE4,
  KB_KEY_CODE5,
  KB_KEY_CODE6,
  KB_REPORT_COUNT,
};

extern bool usb_ready;

extern struct k_msgq msgq_key;
extern struct k_msgq msgq_kb_report;
extern struct k_msgq msgq_peripheral_report;
extern struct k_msgq msgq_key_codes;

extern enum led_color led_state;
extern struct k_sem ble_init_sem;
extern struct k_sem usb_sem;
extern uint16_t pc_interval;

void pos_reset(struct pos *button);
uint8_t uint_get_x(uint8_t num);
uint8_t uint_get_y(uint8_t num);
bool usb_connected();

#endif
