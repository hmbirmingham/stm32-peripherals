# 02-uart

Register-level UART transmit driver on the STM32F411RE Nucleo board. No HAL, no libraries — pure register-level C.

## What it does
Transmits "Hello from STM32" repeatedly over USART2 at 115200 baud by directly configuring the UART peripheral registers via memory-mapped I/O.

## How it works

### 1. Enable clocks
Every peripheral on the STM32 is off by default. GPIOA and USART2 clocks are enabled via the RCC registers before anything else. Skipping this step is the most common mistake — the peripheral simply won't respond.

### 2. Configure PA2 as alternate function
PA2 is the USART2 TX pin on the Nucleo. It must be set to alternate function mode (binary 10 in MODER) so the USART2 peripheral drives it instead of the GPIO output register.

### 3. Set alternate function to AF7
The AFRL register selects which peripheral connects to each pin. AF7 maps PA2 to USART2 TX — this is found in the STM32F411 datasheet alternate function table.

### 4. Set baud rate
BRR = clock frequency / baud rate = 16,000,000 / 115,200 = 138.88. The integer part (138) goes in the mantissa field, the fractional part (0.88 × 16 = 14) goes in the fraction field. Both sides of the connection must use the same baud rate or output is garbled.

### 5. Enable USART2 and TX
Bit 13 of CR1 enables the peripheral. Bit 3 enables the transmitter. Both must be set before transmission begins.

### 6. Transmit
Before writing each byte to the data register, the TXE flag (bit 7 of SR) is polled until the transmit buffer is empty. The hardware then shifts the byte out one bit at a time at 115200 baud.

## Key registers

| Register | Address | Purpose |
|---|---|---|
| RCC_AHB1ENR | 0x40023830 | Enable GPIOA clock |
| RCC_APB1ENR | 0x40023840 | Enable USART2 clock |
| GPIOA_MODER | 0x40020000 | Set PA2 to alternate function mode |
| GPIOA_AFRL | 0x40020020 | Set PA2 alternate function to AF7 (USART2) |
| USART2_BRR | 0x40004408 | Baud rate register — set to 115200 |
| USART2_CR1 | 0x4000440C | Enable USART2 and TX |
| USART2_SR | 0x40004400 | Status register — poll TXE before each write |
| USART2_DR | 0x40004404 | Data register — write byte to transmit |

## Hardware
- STM32 Nucleo-F411RE
- USART2 TX on PA2
- CP2102 USB-UART adapter for serial monitoring on Mac
- Flashed via st-flash over Raspberry Pi

## Build
Compiled with ARM GCC toolchain in STM32CubeIDE targeting ARM Cortex-M4.

## What I learned
- How UART works at the register level — clock enable, GPIO alternate function, baud rate calculation, TX polling
- The alternate function system on STM32 — each pin can connect to multiple peripherals, AFRL selects which one
- Why baud rate must match on both sides — mismatched rates produce garbled output
- The pattern for bringing up any STM32 peripheral: enable clock → configure GPIO → set alternate function → configure peripheral → enable → use