#ifndef MATRIX_H
#define MATRIX_H

#include "../bus/bus.h"
#include <inttypes.h>
#include <stdint.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

#define ROW1_NODE DT_ALIAS(row1)
#define ROW2_NODE DT_ALIAS(row2)
#define ROW3_NODE DT_ALIAS(row3)
#define ROW4_NODE DT_ALIAS(row4)
#define ROW5_NODE DT_ALIAS(row5)

#define COL1_NODE DT_ALIAS(col1)
#define COL2_NODE DT_ALIAS(col2)
#define COL3_NODE DT_ALIAS(col3)
#define COL4_NODE DT_ALIAS(col4)
#define COL5_NODE DT_ALIAS(col5)
#define COL6_NODE DT_ALIAS(col6)

#define X_SIZE ROW_SIZE
#define Y_SIZE COL_SIZE

// gpio defs
extern const struct gpio_dt_spec row1;
extern const struct gpio_dt_spec row2;
extern const struct gpio_dt_spec row3;
extern const struct gpio_dt_spec row4;
extern const struct gpio_dt_spec row5;

extern const struct gpio_dt_spec col1;
extern const struct gpio_dt_spec col2;
extern const struct gpio_dt_spec col3;
extern const struct gpio_dt_spec col4;
extern const struct gpio_dt_spec col5;
extern const struct gpio_dt_spec col6;

extern int values_table[Y_SIZE][X_SIZE];

struct matrix {
  struct gpio_dt_spec rows[ROW_SIZE];
  struct gpio_dt_spec cols[COL_SIZE];
};

// handlers
void scan_all(struct matrix *mtx);

void keyboard_task(void *p1, void *p2, void *p3);

void scan_row(struct matrix *mtx, int num);

void process_click(struct pos *position);
#endif
