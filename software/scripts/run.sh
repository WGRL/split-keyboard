#!/usr/bin/env bash
set -e

BOARD=${BOARD:-xiao_ble} 

if [ "$1" = "left" ]; then
    CONF_FILE="config/prj_left.conf"
    BUILD_DIR="build_left"
    OVERLAY_FILE="boards/left.overlay"
    echo "left"
else 
    CONF_FILE="config/prj_right.conf"
    BUILD_DIR="build_right"
    OVERLAY_FILE="boards/right.overlay"
    echo "right"
fi

rm -rf "$BUILD_DIR"

west build -b "$BOARD" -p always -d "$BUILD_DIR" \
  -- -DCONF_FILE="config/prj.conf;$CONF_FILE"\

cp "$BUILD_DIR/zephyr/zephyr.uf2" "/media/$USER/XIAO-SENSE/"

cp "$BUILD_DIR/compile_commands.json" "build/compile_commands.json"
