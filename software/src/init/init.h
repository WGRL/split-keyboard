#ifndef INIT_H
#define INIT_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define GPIO_INIT_ERR -1

struct all_pins {
  uint8_t led_num;
  uint8_t row_num;
  uint8_t col_num;

  struct gpio_dt_spec *leds;
  struct gpio_dt_spec *rows;
  struct gpio_dt_spec *cols;
};

int init_validation(struct all_pins *pins);

void init_configure(struct all_pins *pins);

#endif
