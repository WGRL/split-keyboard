#ifndef LEDS_H
#define LEDS_H

#include "../bus/bus.h"
#include <hal/nrf_power.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define LED_SIZE 4

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

// #define LED3_NODE DT_ALIAS(led3)

extern const struct gpio_dt_spec led_red;
extern const struct gpio_dt_spec led_green;
extern const struct gpio_dt_spec led_blue;

void underglow();
void led_task(void *p1, void *p2, void *p3);

#endif
