@echo off
setlocal
cd /d "%~dp0"

set "TOOLCHAIN=C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\12.2 mpacbti-rel1\bin"

if /I "%~1"=="dfu" goto build_usb_demo
if /I "%~1"=="blink" goto build_blink_demo
if /I "%~1"=="usb" goto build_usb_demo

"%TOOLCHAIN%\arm-none-eabi-gcc.exe" -mcpu=cortex-m4 -mthumb -O2 -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns -nostdlib -Tlink.ld pwm.c -o esc.elf
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-objcopy.exe" -O binary esc.elf esc.bin
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-size.exe" esc.elf
if errorlevel 1 exit /b 1

echo.
echo Build complete: esc.bin
exit /b 0

:build_usb_demo
"%TOOLCHAIN%\arm-none-eabi-gcc.exe" -mcpu=cortex-m4 -mthumb -DSTM32F401xE -O2 -ffreestanding -fno-builtin -Wall -Wextra -nostdlib -nostartfiles -I. -Ithird_party\tinyusb\src -Ithird_party\tinyusb\src\portable\synopsys\dwc2 -Ithird_party\cmsis_device_f4\Include -Ithird_party\CMSIS_5\CMSIS\Core\Include -TSTM32F401RCT6.ld -Wl,-Map=usb_cdc.map bootlading.c startup.s third_party\tinyusb\src\tusb.c third_party\tinyusb\src\common\tusb_fifo.c third_party\tinyusb\src\device\usbd.c third_party\tinyusb\src\class\cdc\cdc_device.c third_party\tinyusb\src\portable\synopsys\dwc2\dwc2_common.c third_party\tinyusb\src\portable\synopsys\dwc2\dcd_dwc2.c -lc -lgcc -o usb_cdc.elf
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-objcopy.exe" -O ihex usb_cdc.elf usb_cdc.hex
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-objcopy.exe" -O binary usb_cdc.elf usb_cdc.bin
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-size.exe" usb_cdc.elf
if errorlevel 1 exit /b 1

call :build_blink_demo
if errorlevel 1 exit /b 1

echo.
echo Build complete: usb_cdc.hex and blink.bin
exit /b 0

:build_blink_demo
"%TOOLCHAIN%\arm-none-eabi-gcc.exe" -mcpu=cortex-m4 -mthumb -O2 -ffreestanding -fno-builtin -c startup.s -o startup.o
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-gcc.exe" -mcpu=cortex-m4 -mthumb -O2 -ffreestanding -fno-builtin -Wall -Wextra -c blink.c -o blink.o
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-gcc.exe" -mcpu=cortex-m4 -mthumb -nostdlib -nostartfiles -Wl,-Map=blink.map -TSTM32F401RCT6.ld blink.o startup.o -o blink.elf
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-objcopy.exe" -O binary blink.elf blink.bin
if errorlevel 1 exit /b 1

"%TOOLCHAIN%\arm-none-eabi-size.exe" blink.elf
if errorlevel 1 exit /b 1

echo.
echo Build complete: blink.bin
exit /b 0
