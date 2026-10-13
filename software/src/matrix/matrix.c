#include "matrix.h"
#include "syscalls/kernel.h"
#include <string.h>

#define NO_KEY 255

#define FREQENCY K_MSEC(30)
#define DEBOUNCE_CHECK 5

#define POSITION_LIMIT 250

#define MOD_CLEAR 0x0
#define MOD_L_CTRL 0x1
#define MOD_L_SHIFT 0x2
#define MOD_L_ALT 0x4
#define MOD_L_SUP 0x8
#define MOD_R_ALT 0x40

#ifdef CONFIG_KEYBOARD_SIDE_LEFT
#define COMMUNICATION_FREQ COMM_FREQ_LEFT
#else
#define COMMUNICATION_FREQ COMM_FREQ_RIGHT
#endif

// gpio defs
const struct gpio_dt_spec row1 = GPIO_DT_SPEC_GET(ROW1_NODE, gpios);
const struct gpio_dt_spec row2 = GPIO_DT_SPEC_GET(ROW2_NODE, gpios);
const struct gpio_dt_spec row3 = GPIO_DT_SPEC_GET(ROW3_NODE, gpios);
const struct gpio_dt_spec row4 = GPIO_DT_SPEC_GET(ROW4_NODE, gpios);
const struct gpio_dt_spec row5 = GPIO_DT_SPEC_GET(ROW5_NODE, gpios);

const struct gpio_dt_spec col1 = GPIO_DT_SPEC_GET(COL1_NODE, gpios);
const struct gpio_dt_spec col2 = GPIO_DT_SPEC_GET(COL2_NODE, gpios);
const struct gpio_dt_spec col3 = GPIO_DT_SPEC_GET(COL3_NODE, gpios);
const struct gpio_dt_spec col4 = GPIO_DT_SPEC_GET(COL4_NODE, gpios);
const struct gpio_dt_spec col5 = GPIO_DT_SPEC_GET(COL5_NODE, gpios);
const struct gpio_dt_spec col6 = GPIO_DT_SPEC_GET(COL6_NODE, gpios);

#ifdef CONFIG_KEYBOARD_SIDE_LEFT
static struct pos l_ctrl = {.x = 4, .y = 0};
static struct pos l_shift = {.x = 3, .y = 0};
static struct pos l_alt = {.x = 4, .y = 2};
static struct pos l_super = {.x = 4, .y = 1};
static struct pos r_alt = {.x = NO_KEY, .y = NO_KEY};
static struct pos caps = {.x = 2, .y = 0};
#else
static struct pos l_ctrl = {.x = NO_KEY, .y = NO_KEY};
static struct pos l_shift = {.x = NO_KEY, .y = NO_KEY};
static struct pos l_alt = {.x = NO_KEY, .y = NO_KEY};
static struct pos l_super = {.x = NO_KEY, .y = NO_KEY};
static struct pos r_alt = {.x = 4, .y = 3};
static struct pos caps = {.x = NO_KEY, .y = NO_KEY};
#endif

struct pos keys_clicked[HID_LIM];
static uint8_t hid_kpos;

static struct pos ready_to_send[HID_LIM];
static uint8_t rts_idx; // ready_to_send index
//
// static void print_pos(struct pos *obj) {
//   printk("x: %d y: %d side: %d state: %d modifiers: %x\n", obj->x, obj->y,
//          obj->side, obj->state, obj->modifier);
// }

static uint8_t debounce_table[X_SIZE][Y_SIZE];

static bool clicked_this_round(uint8_t x, uint8_t y) {
  for (int i = 0; i < hid_kpos; i++) {
    if (keys_clicked[i].x == x && keys_clicked[i].y == y) {
      return 1;
    }
  }
  return 0;
}

static void verify_clicked() {
  for (int i = 0; i < X_SIZE; i++) {
    for (int j = 0; j < Y_SIZE; j++) {
      if (clicked_this_round(i, j)) {
        debounce_table[i][j]++;
        continue;
      }
      debounce_table[i][j] = 0;
    }
  }
}

void keyboard_task(void *p1, void *p2, void *p3) {
  struct matrix mtx = {
      .rows =
          {
              GPIO_DT_SPEC_GET(ROW1_NODE, gpios),
              GPIO_DT_SPEC_GET(ROW2_NODE, gpios),
              GPIO_DT_SPEC_GET(ROW3_NODE, gpios),
              GPIO_DT_SPEC_GET(ROW4_NODE, gpios),
              GPIO_DT_SPEC_GET(ROW5_NODE, gpios),
          },
      .cols =
          {
              GPIO_DT_SPEC_GET(COL1_NODE, gpios),
              GPIO_DT_SPEC_GET(COL2_NODE, gpios),
              GPIO_DT_SPEC_GET(COL3_NODE, gpios),
              GPIO_DT_SPEC_GET(COL4_NODE, gpios),
              GPIO_DT_SPEC_GET(COL5_NODE, gpios),
              GPIO_DT_SPEC_GET(COL6_NODE, gpios),
          },

  };

  memset(debounce_table, 0, sizeof(debounce_table));
  // dla last clicked
  rts_idx = 0;
  memset(ready_to_send, 1, sizeof(ready_to_send));

  while (1) {
    scan_all(&mtx);
    k_sleep(K_MSEC(MATRIX_FREQ));
  }
}

static void detect_color_change(int x, int y) {
  if (x == caps.x && y == caps.y) {
    led_state = white;
  } else {
    led_state = green;
  }
}

void process_click(struct pos *position) {
#ifdef CONFIG_KEYBOARD_SIDE_LEFT
  position->side = left;
#else
  position->side = right;
#endif

  if (position->x > XY_LIMIT)
    return; // all set to 1 by default
  if (hid_kpos == 0)
    return;
  while (k_msgq_put(&msgq_key, position, K_NO_WAIT) != RET_FINE) {
    /* message queue is full: purge old data & try again */
    k_msgq_purge(&msgq_key);
  }
}

// returns pos in clicked by x and y.
// used to regain modifiers
struct pos return_pos(uint8_t x, uint8_t y) {
  struct pos ans;
  memset(&ans, 1, sizeof(ans));
  for (int i = 0; i < HID_LIM; i++) {
    if (keys_clicked[i].x == x && keys_clicked[i].y == y)
      return keys_clicked[i];
  }
  return ans;
}

static void prepare_scan(void) {
  memset(keys_clicked, 1, sizeof(keys_clicked));
  hid_kpos = 0;

  rts_idx = 0;
  memset(ready_to_send, 1, sizeof(ready_to_send));
}

static void scan_matrix_rows(struct matrix *mtx) {
  for (char i = 0; i < ROW_SIZE; i++) {
    scan_row(mtx, i);
  }
}

static void build_ready_to_send_list(void) {
  struct pos position;

  verify_clicked();

  for (int i = 0; i < X_SIZE; i++) {
    for (int j = 0; j < Y_SIZE; j++) {
      if (debounce_table[i][j] > DEBOUNCE_CHECK) {
        position = return_pos(i, j);

        if (position.x >= XY_LIMIT)
          continue;

        ready_to_send[rts_idx] = position;

        // ready_to_send[rts_idx].state = pressed;
        // print_pos(&ready_to_send[rts_idx]);
        rts_idx++;
      }
    }
  }
}

static void process_ready_positions(void) {

  for (int i = 0; i < rts_idx; i++) {
    process_click(&ready_to_send[i]);
  }
}

void scan_all(struct matrix *mtx) {
  prepare_scan();

  scan_matrix_rows(mtx);

  build_ready_to_send_list();

  process_ready_positions();
}

static uint8_t set_modifiers(uint8_t x, uint8_t y) {
  uint8_t modifiers = MOD_CLEAR;
  detect_color_change(l_ctrl.x, l_ctrl.y);
  if (x == l_ctrl.x && y == l_ctrl.y) {
    modifiers |= MOD_L_CTRL;
  } else if (x == l_shift.x && y == l_shift.y) {
    modifiers |= MOD_L_SHIFT;
  } else if (x == l_alt.x && y == l_alt.y) {
    modifiers |= MOD_L_ALT;
  } else if (x == l_super.x && y == l_super.y) {
    modifiers |= MOD_L_SUP;
  } else if (x == r_alt.x && y == r_alt.y) {
    modifiers |= MOD_R_ALT;
  }
  return modifiers;
}

void scan_row(struct matrix *mtx, int num) {
  int clicked;

  gpio_pin_set_dt(&(mtx->rows[num]), 1);

  for (int i = 0; i < COL_SIZE; i++) {
    clicked = gpio_pin_get_dt(&(mtx->cols[i]));

    if (clicked == 1) {
      // to avoid overflow
      if (debounce_table[num][i] < DEBOUNCE_CHECK)
        debounce_table[num][i]++;
      keys_clicked[hid_kpos].x = num;
      keys_clicked[hid_kpos].y = i;
      keys_clicked[hid_kpos].modifier = set_modifiers(num, i);
      hid_kpos++;
    }
  }
  gpio_pin_set_dt(&(mtx->rows[num]), 0);
}
