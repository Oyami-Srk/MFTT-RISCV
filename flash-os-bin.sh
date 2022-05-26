#!/bin/sh
./tools/ktool/ktool.py -p /dev/ttyUSB0 -b 3000000 ./cmake-build-debug/rustsbi-k210.bin -a 0x80000000

./tools/ktool/ktool.py -p /dev/ttyUSB0 -b 3000000 -t ./cmake-build-debug/kernel.bin -a 0x80020000 -s
