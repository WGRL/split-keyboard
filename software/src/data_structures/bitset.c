#include "bitset.h"

void insert_set(Set *set, int val) {
  if (val >= SET_MAX)
    return;
  uint8_t idx = val / INTERVAL;
  set->bits[idx] |= 1 << (val % INTERVAL);
}

void remove_set(Set *set, int val) {
  if (val >= SET_MAX)
    return;
  uint8_t idx = val / INTERVAL;
  set->bits[idx] &= ~(1 << (val % INTERVAL));
}

void clear_set(Set *set) { memset(set->bits, 0, sizeof(set->bits)); }

bool contains_set(Set *set, uint8_t val) {
  uint8_t idx = val / INTERVAL;
  return (set->bits[idx] & (1 << (val % INTERVAL))) != 0;
}

size_t set_get_values(const Set *set, int *out) {
  size_t count = 0;

  for (int i = 0; i < SET_MAX; ++i) {
    int byte_idx = i / 8;
    int bit_idx = i % 8;

    if (set->bits[byte_idx] & (1u << bit_idx)) {
      out[count++] = i;
    }
  }

  return count;
}
