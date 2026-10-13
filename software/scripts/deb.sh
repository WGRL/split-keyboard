#!/usr/bin/env bash
# I forgor this every single time


if [ "$1" = "no" ]; then
    screen /dev/ttyACM0 115200
else
    screen -L -Logfile plik.log /dev/ttyACM0 115200
fi
