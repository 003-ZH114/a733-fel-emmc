#!/bin/sh
# Build the standalone A733 FEL eMMC initialization payload.
#
# Output: build/a733-init.bin  (link address 0x57000, load with xfel)
#
# Requires the ARM bare-metal toolchain (arm-none-eabi-gcc / ld / objcopy).

set -e

CC=arm-none-eabi-gcc
LD=arm-none-eabi-ld
OBJCOPY=arm-none-eabi-objcopy
OBJDUMP=arm-none-eabi-objdump

CFLAGS="-nostdlib -nostartfiles -ffreestanding -O2 -march=armv7-a \
        -DSPEED_1BIT_FAST -I02-clock -I03-emmc-init"

mkdir -p build

$CC $CFLAGS -c 01-startup/start.S          -o build/start.o
$CC $CFLAGS -c 02-clock/clock_config.c     -o build/clock_config.o
$CC $CFLAGS -c 03-emmc-init/emmc_init.c    -o build/emmc_init.o
$CC $CFLAGS -c 03-emmc-init/init_entry.c   -o build/init_entry.o

$LD -T 01-startup/a733.ld --build-id=none -o build/a733-init.elf \
    build/start.o build/clock_config.o build/emmc_init.o build/init_entry.o

$OBJCOPY -O binary build/a733-init.elf build/a733-init.bin
$OBJDUMP -d build/a733-init.elf > build/a733-init.dis

echo "built: build/a733-init.bin"
