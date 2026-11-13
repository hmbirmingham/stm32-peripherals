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

02-uart builds with make and needs no IDE. The ARM toolchain ships inside
STM32CubeIDE, so the Makefile looks for it there, or uses `arm-none-eabi-gcc`
from PATH if you have it installed separately.

```
cd 02-uart
make
make flash
```

`make flash` writes the binary to 0x08000000 over ST-LINK. `make clean` removes
the build directory. Pass `TOOLCHAIN=` if your toolchain lives somewhere the
Makefile does not look.

01-blinky has no build system. Its CubeIDE project files were removed early on,
so it is kept as a register level reference rather than something I rebuild.

## Testing the baud rate calculation

The BRR helper is pure arithmetic, so it runs on the host with no board
attached.

```
cd 02-uart
make test
```

## What is next

My next work is a separate repository for a systolic array and 3D sensing.
