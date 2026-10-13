#include "init.h"

static int validate_arr(struct gpio_dt_spec *arr, int len, char *name) {
  for (int i = 0; i < len; i++) {
    if (!gpio_is_ready_dt(&arr[i])) {
      printk("%s%d GPIO not ready\n", name, i);
      return GPIO_INIT_ERR;
    }
  }
  return 0;
}

int init_validation(struct all_pins *pins) {
  validate_arr(pins->leds, pins->led_num, "LED");

  validate_arr(pins->cols, pins->col_num, "COL");

  validate_arr(pins->rows, pins->row_num, "ROW");
  return 0;
}

void init_configure(struct all_pins *pins) {
  int i;

  for (i = 0; i < pins->row_num; i++)
    gpio_pin_configure_dt(&pins->rows[i], GPIO_OUTPUT_INACTIVE);

  for (i = 0; i < pins->col_num; i++)
    gpio_pin_configure_dt(&pins->cols[i], GPIO_INPUT | GPIO_PULL_UP);

  // built-in leds and external led controller led_din
  for (i = 0; i < pins->led_num; i++)
    gpio_pin_configure_dt(&pins->leds[i], GPIO_OUTPUT_ACTIVE);
}
