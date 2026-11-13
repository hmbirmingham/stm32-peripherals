# stm32-peripherals

Bare metal peripheral drivers for the STM32F411RE Nucleo board, written at the
register level with no HAL, no CMSIS and no vendor headers. I built this to learn
peripheral bring up from first principles, working from the reference manual and
raw memory addresses instead of generated code.

## Projects

| Folder | Peripheral | Status |
|---|---|---|
| 01-blinky | GPIO, register level LED toggle on PA5 | Complete |
| 02-uart | USART2, interrupt driven TX at 115200 baud | Complete |

Both are confirmed on hardware. I confirmed 02-uart on 2026-11-13 by serial
decode and by reading the driver state over SWD. The one thing still missing is
a logic analyzer capture of PA2, which is documentation rather than proof.
`02-uart/README.md` says exactly what was measured.

## Hardware

- STM32 Nucleo-F411RE
- ST-LINK virtual COM port for serial monitoring, over the board's own USB cable
- Logic analyzer for bus verification, once I have a cable for it

I usually flash from a Raspberry Pi with st-flash, but it works the same
directly from the Mac.

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
