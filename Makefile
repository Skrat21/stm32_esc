TOOLCHAIN = arm-none-eabi-
CC = $(TOOLCHAIN)gcc
OBJCOPY = $(TOOLCHAIN)objcopy
SIZE = $(TOOLCHAIN)size

CPUFLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CFLAGS = $(CPUFLAGS) -std=c11 -O2 -ffreestanding -fno-builtin -Wall -Wextra
LDFLAGS = $(CPUFLAGS) -nostdlib -nostartfiles -Wl,-Map=pwm.map -TSTM32F401RCT6.ld

.PHONY: all clean dfu-demo usb-demo

all: pwm.hex

pwm.elf: pwm.c startup.s STM32F401RCT6.ld
	$(CC) $(CFLAGS) -c startup.s -o startup.o
	$(CC) $(CFLAGS) -c pwm.c -o pwm.o
	$(CC) $(LDFLAGS) pwm.o startup.o -o $@
	$(SIZE) $@

pwm.hex: pwm.elf
	$(OBJCOPY) -O ihex $< $@

TINYUSB = third_party/tinyusb/src
CMSIS_DEVICE = third_party/cmsis_device_f4/Include
CMSIS_CORE = third_party/CMSIS_5/CMSIS/Core/Include
USB_SOURCES = $(TINYUSB)/tusb.c \
              $(TINYUSB)/common/tusb_fifo.c \
              $(TINYUSB)/device/usbd.c \
              $(TINYUSB)/class/cdc/cdc_device.c \
              $(TINYUSB)/portable/synopsys/dwc2/dwc2_common.c \
              $(TINYUSB)/portable/synopsys/dwc2/dcd_dwc2.c

usb_cdc.elf: bootlading.c tusb_config.h startup.s STM32F401RCT6.ld $(USB_SOURCES)
	$(CC) $(CFLAGS) -DSTM32F401xE -I. -I$(TINYUSB) -I$(TINYUSB)/portable/synopsys/dwc2 -I$(CMSIS_DEVICE) -I$(CMSIS_CORE) -nostdlib -nostartfiles -Wl,-Map=usb_cdc.map -TSTM32F401RCT6.ld bootlading.c startup.s $(USB_SOURCES) -lc -lgcc -o $@
	$(SIZE) $@

usb_cdc.hex: usb_cdc.elf
	$(OBJCOPY) -O ihex $< $@

blink.elf: blink.c startup.s STM32F401RCT6.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-Map=blink.map -TSTM32F401RCT6.ld blink.c startup.s -o $@
	$(SIZE) $@

blink.bin: blink.elf
	$(OBJCOPY) -O binary $< $@

usb_cdc.bin: usb_cdc.elf
	$(OBJCOPY) -O binary $< $@

dfu-demo usb-demo: usb_cdc.hex usb_cdc.bin blink.bin

clean:
	rm -f startup.o pwm.o bootlading.o pwm.elf pwm.hex pwm.map usb_cdc.elf usb_cdc.hex usb_cdc.bin usb_cdc.map blink.o blink.elf blink.hex blink.bin blink.map dfu_demo.elf dfu_demo.hex dfu_demo.map
