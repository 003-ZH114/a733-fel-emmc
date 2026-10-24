#!/bin/sh
# Build the full A733 FEL eMMC read/write payload.
# Output: build/fel.bin (link address 0x57000)

set -e
cd "$(dirname "$0")"
mkdir -p build

arm-none-eabi-gcc -nostdlib -nostartfiles -ffreestanding -O2 \
  -DSPEED_1BIT_FAST -march=armv7-a -c fel-emmc-a733.c -o build/fel.o
arm-none-eabi-ld -T ../01-startup/a733.ld --build-id=none -o build/fel.elf build/fel.o
arm-none-eabi-objcopy -O binary build/fel.elf build/fel.bin
arm-none-eabi-objdump -d build/fel.elf > build/payload.dis

echo "built: build/fel.bin"
