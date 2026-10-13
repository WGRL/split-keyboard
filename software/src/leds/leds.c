#include "leds.h"

const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
// const struct gpio_dt_spec led_din = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

const struct device *strip = DEVICE_DT_GET(DT_NODELABEL(led_strip));
static struct led_rgb pixels[LED_SIZE];

static void set_color(int r, int g, int b) {

  for (int i = 0; i < LED_SIZE; i++) {
    pixels[i].r = r;
    pixels[i].g = g;
    pixels[i].b = b;
  }
  int ret = led_strip_update_rgb(strip, pixels, LED_SIZE);
  // printk("ret=%d\n", ret);
}

// potem zrob osobne flagi, zeby kolory mogly sie mieszac
// albo zeby bylo pol na pol, tez cool
static void color_switch() {
  switch (led_state) {
  case black:
    set_color(0, 0, 0);
    break;
  case white:
    set_color(255, 255, 255);
    break;
  case red:
    set_color(255, 0, 0);
    break;
  case green:
    set_color(0, 255, 0);
    break;
  case blue:
    set_color(0, 0, 255);
    break;
  }
}

void led_task(void *p1, void *p2, void *p3) {
  led_state = black;
  while (1) {
    underglow();
    color_switch();
    k_sleep(K_MSEC(200));
  }
}

void underglow() {
  bool usb_status = nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
  if (!usb_status)
    gpio_pin_set_dt(&led_red, 1);
  else {
    gpio_pin_set_dt(&led_red, 1);
  }
}
