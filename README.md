# stm32-peripherals

Bare metal peripheral drivers for the STM32F411RE Nucleo board, written at the
register level with no HAL, no CMSIS and no vendor headers. I built this to learn
peripheral bring up from first principles, working from the reference manual and
raw memory addresses instead of generated code.

## Projects

| Folder | Peripheral | Status |
|---|---|---|
| 01-blinky | GPIO, register level LED toggle on PA5 | Complete |
| 02-uart | USART2, interrupt driven TX at 115200 baud | In Progress |

02-uart stays In Progress until I confirm the interrupt driven driver on hardware
and save a logic analyzer capture of PA2. The driver and the baud rate
calculation are written, and the calculation has a host test that passes.

## Hardware

- STM32 Nucleo-F411RE
- CP2102 USB UART adapter for serial monitoring
- Logic analyzer for bus verification

## Toolchain

ARM GCC, STM32CubeIDE, st-flash, macOS

## Build and flash

I build in STM32CubeIDE, which generates the makefile and the linker scripts
under each project's `Debug` directory. Neither is committed, so a fresh clone
has to be built from the IDE before there is anything to flash.

1. Open the project folder in STM32CubeIDE
2. Project, then Build All
3. Flash the resulting binary with st-flash

```
st-flash --reset write Debug/02-uart.bin 0x08000000
```

## Testing the baud rate calculation

The BRR helper is pure arithmetic, so it runs on the host with no board
attached.

```
cd 02-uart
cc -std=c11 -Wall -Wextra -o /tmp/test_brr test/test_brr.c
/tmp/test_brr
```

## What is next

My next work is a separate repository for a systolic array and 3D sensing.
