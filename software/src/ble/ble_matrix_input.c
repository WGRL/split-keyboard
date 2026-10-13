#include "ble_matrix_input.h"

#define MAX_KEYS HID_LIM
#define KEY_LIST_START 2

static uint8_t get_key_value(struct pos button) {
  if (button.side == left)
    return values_table_l[button.x][button.y];
  else if (button.side == right)
    return values_table_r[button.x][button.y];

  return 0;
}

static void collect_pressed_keys(Set *set, uint8_t *modifier) {
  int8_t ret;
  uint8_t key_val = 1;
  struct pos button;

  while (key_val != 0) {
    ret = k_msgq_get(&msgq_key, &button, K_NO_WAIT);

    if (button.x > XY_LIMIT)
      continue;

    if (ret != RET_FINE)
      break;

    // printk("%d %d", button.x, button.y);

    key_val = get_key_value(button);

    *modifier |= button.modifier;

    if (!contains_set(set, key_val)) {
      insert_set(set, key_val);
    }
  }
}

static size_t copy_keys_from_set(Set *set, struct key_code *keys,
                                 uint8_t modifier) {
  int values[SET_MAX];

  size_t n = set_get_values(set, values);

  if (n > HID_LIM) {
    n = HID_LIM;
  }

  for (size_t i = 0; i < n; i++) {
    struct key_code key = {
        .value = values[i],
        .modifier = modifier,
    };

    keys[i] = key;
  }

  return n;
}

size_t get_all_queue_l(struct key_code *keys) {
  Set set_obj;
  Set *set = &set_obj;

  uint8_t modifier = 0;

  clear_set(set);

  collect_pressed_keys(set, &modifier);

  return copy_keys_from_set(set, keys, modifier);
}

void create_report(uint8_t report[KB_REPORT_COUNT]) {
  memset(report, 0, sizeof(report[0] * KB_REPORT_COUNT));

  Set clicked;
  clear_set(&clicked);

  uint8_t ret;
  uint8_t i = 0;
  struct key_code button;

  while (i < KB_REPORT_COUNT - KB_KEY_CODE1) {
    ret = k_msgq_get(&msgq_key_codes, &button, K_NO_WAIT);
    if (ret != RET_FINE)
      break;

    if (!contains_set(&clicked, button.value)) {
      insert_set(&clicked, button.value);
      report[KB_MOD_KEY] |= button.modifier;
      report[KB_KEY_CODE1 + i] = button.value;
      i++;
    }

    memset(&button, 0, sizeof(button));
    // pos_reset(&tx_value_master);
  }
  report[KB_RESERVED] = 0;
}
