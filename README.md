# 01-blinky

Bare-metal LED blink on the STM32F411RE Nucleo board. No HAL, no libraries, pure register-level C.

## What it does
Toggles the onboard green LED (PA5) at roughly 1Hz by directly writing to the GPIO peripheral registers via memory-mapped I/O.

## How it works
The STM32F411RE exposes all hardware peripherals as memory-mapped registers. To blink the LED:

1. Enable the GPIOA clock via the RCC AHB1 enable register (`0x40023830`)
2. Configure PA5 as a general-purpose output via the MODER register (`0x40020000`)
3. Toggle bit 5 of the ODR register (`0x40020014`) in an infinite loop with a software delay

No HAL, no CMSIS, no vendor headers — just `stdint.h` and direct memory access.

## Hardware
- STM32 Nucleo-F411RE
- Onboard green LED on PA5
- Flashed via st-flash over Raspberry Pi (ST-Link V2 firmware update pending)

## Build
Compiled with ARM GCC toolchain in STM32CubeIDE targeting ARM Cortex-M4.

## Key registers
| Register | Address | Purpose |
|---|---|---|
| RCC_AHB1ENR | 0x40023830 | Enable GPIOA clock |
| GPIOA_MODER | 0x40020000 | Set PA5 as output |
| GPIOA_ODR | 0x40020014 | Toggle PA5 |

## What I learned
- How to configure GPIO from scratch using only the reference manual and memory addresses
- The role of `volatile` in preventing compiler optimization of hardware register writes
- How to set up and use the ARM GCC toolchain on macOS
