cmake_minimum_required(VERSION 3.20.0)
set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Keyboard)

target_sources(app PRIVATE
  src/main.c
  src/matrix/matrix.c
  src/leds/leds.c
  src/init/init.c
  src/bus/bus.c
  src/data_structures/bitset.c
  src/ble/ble_matrix_input.c
)

target_sources_ifdef(CONFIG_KEYBOARD_SIDE_LEFT app PRIVATE
  src/ble/ble_client.c
  src/usb/usb.c
  src/usb/usb_init.c
  src/ble/ble_hid.c
  src/ble/hog.c
  src/usb_ble_event/usb_ble_event.c
)

target_sources_ifdef(CONFIG_KEYBOARD_SIDE_RIGHT app PRIVATE
  src/ble/ble_peripheral.c)

message(STATUS "\n\n\n\n\n")
message(STATUS "LEFT=${CONFIG_KEYBOARD_SIDE_LEFT}")
message(STATUS "RIGHT=${CONFIG_KEYBOARD_SIDE_RIGHT}")
message(STATUS "\n\n\n\n\n")
