#include "init/init.h"
#include "leds/leds.h"
#include "matrix/matrix.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#ifdef CONFIG_KEYBOARD_SIDE_LEFT
#include "ble/ble_client.h"
#include "ble/ble_hid.h"
#include "usb/usb.h"
#include "usb_ble_event/usb_ble_event.h"

#define TEXT "LEFT\n"
#else
#include "ble/ble_peripheral.h"
#define TEXT "RIGHT\n"
#endif

K_THREAD_DEFINE(keyboard_tid, 1024, keyboard_task, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(led_tid, 1024, led_task, NULL, NULL, NULL, 2, 0, 0);

#ifdef CONFIG_KEYBOARD_SIDE_LEFT
K_THREAD_DEFINE(ble_tid, 2048, ble_task_master, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(usb_tid, 2048, usb_task, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(ble_pc_tid, 4096, ble_pc_task, NULL, NULL, NULL, 2, 0, 0);
// K_THREAD_DEFINE(swap_tid, 512, swap_task, NULL, NULL, NULL, 2, 0, 0);
#endif

#ifdef CONFIG_KEYBOARD_SIDE_RIGHT
K_THREAD_DEFINE(ble_tid, 2048, ble_task_slave, NULL, NULL, NULL, 2, 0, 0);
#endif

int main(void) {

  // data for validation, not use
  struct gpio_dt_spec leds[] = {led_red, led_green, led_blue};
  struct gpio_dt_spec rows[] = {row1, row2, row3, row4, row5};
  struct gpio_dt_spec cols[] = {col1, col2, col3, col4, col5, col6};

  struct all_pins pins = {
      .leds = leds,
      .led_num = 3,
      .rows = rows,
      .row_num = ROW_SIZE,
      .cols = cols,
      .col_num = COL_SIZE,
  };

  // check if the pins are fine
  init_configure(&pins);
  if (init_validation(&pins) == -1)
    return -1;

  // int x = 0;
  // while (true) {
  //   if (x % 2)
  //     led_state = red;
  //   else
  //     led_state = blue;
  //   x++;
  //   k_sleep(K_SECONDS(1));
  // }
}
