#ifndef BITSET_H
#define BITSET_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define SET_MAX 256 // maximum value is SET_MAX-1
#define INTERVAL 8

typedef struct {
  uint8_t bits[SET_MAX / INTERVAL];
} Set;

void insert_set(Set *set, int val);

void remove_set(Set *set, int val);

void clear_set(Set *set);

bool contains_set(Set *set, uint8_t val);

size_t set_get_values(const Set *set, int *out);

#endif
