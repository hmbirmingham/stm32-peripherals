# 02-uart

Interrupt driven USART2 transmit driver for the STM32F411RE Nucleo board, written
at the register level with no HAL.

## Status

Complete, with one piece of documentation outstanding.

I confirmed this on hardware on 2026-11-13. The driver transmits correctly and
the error handling works. What I have not done is measure the bit period
directly with a logic analyzer, because I do not have the cable for it. The baud
rate is confirmed indirectly instead, by an independent receiver decoding it
cleanly. The results below say exactly what was and was not measured.

## Build

```
make
make flash
```

No IDE needed. The Makefile uses the ARM toolchain that ships inside
STM32CubeIDE, or `arm-none-eabi-gcc` from PATH. `make test` runs the host test
for the baud rate calculation with no board attached.

The linker script is the one STM32CubeIDE generated. It is committed because
without it there is nothing to link against.

## Pins

| Signal | Pin | Nucleo header | Alternate function |
|---|---|---|---|
| USART2_TX | PA2 | CN10 pin 35 | AF7 |
| USART2_RX | PA3 | CN10 pin 37 | AF7 |

On the Nucleo-F411RE both pins are also routed to the ST-LINK virtual COM port
through solder bridges. If PA3 is left floating the noise and framing counters
will climb on their own, which is the input floating rather than a fault in the
driver.

## Settings

| Setting | Value |
|---|---|
| Baud | 115200 nominal, 115108 actual |
| Frame | 8 data bits, no parity, 1 stop bit |
| Clock source | 16 MHz HSI, the reset default |
| Oversampling | 16x, OVER8 = 0 |
| BRR | 139, which is 0x8B |
| TX buffer | 64 bytes, interrupt drained |

## Baud rate

BRR holds USARTDIV in 12.4 fixed point, which means the register value is simply
the clock divided by the baud rate, rounded to nearest.

```
BRR = fck / baud = 16000000 / 115200 = 138.9, rounds to 139
```

139 gives an actual baud of 16000000 / 139 = 115108, which is 0.08 percent off
nominal and well inside what a UART tolerates. The expected bit period is
8.69 microseconds and a full 10 bit frame is 86.9 microseconds.

I got this wrong the first time. I wrote the register as `(138 << 4) | 14`,
treating 138 as a mantissa and 14 as a fraction. That double counts the fixed
point scale, because BRR already holds the fraction in its low four bits. The
register ended up at 0x8AE, which is 2222, giving about 7200 baud instead of
115200 and garbled output. `Inc/brr.h` now does the arithmetic and
`test/test_brr.c` pins it down, including a regression check against the old
value.

I also dropped an earlier assumption that USART2 runs from a 42 MHz APB1 clock.
Nothing in this project writes RCC_CFGR, so the chip is still on the 16 MHz HSI
at reset.

## How the driver works

1. Enable the GPIOA and USART2 clocks in RCC. Every peripheral is off at reset
   and this is the step that is easiest to forget.
2. Put PA2 and PA3 into alternate function mode in MODER, clearing each two bit
   field before setting it.
3. Select AF7 for both pins in AFRL, which is what maps them to USART2.
4. Write BRR.
5. Enable USART2 interrupt 38 in the NVIC. ISER1 covers interrupts 32 to 63, so
   the bit is 38 minus 32.
6. Set UE, TE, RE and RXNEIE in CR1.

Transmission goes through a 64 byte ring buffer. `uart_write` copies bytes in,
returns how many it accepted, and sets TXEIE. It never blocks. If the buffer is
full the surplus is dropped and counted in `uart_tx_dropped`, because spinning
there would defeat the point of moving transmission into the interrupt.

`USART2_IRQHandler` writes one byte per TXE event and clears TXEIE once the
buffer drains, otherwise the interrupt would fire continuously with nothing to
send.

The buffer size is a power of two so the wrap is a bitwise mask rather than a
modulo. Only main writes the head index and only the ISR writes the tail, so
with both declared volatile on a single core part this needs no critical
section. That assumption breaks the moment a second writer appears.

## Error handling

Framing, overrun, noise and parity errors are detected in the ISR, each with its
own counter.

| Counter | Meaning |
|---|---|
| `uart_err_framing` | Stop bit was not where it should be, usually a baud mismatch |
| `uart_err_overrun` | A byte arrived before the previous one was read |
| `uart_err_noise` | Sampling disagreed within a bit period |
| `uart_err_parity` | Parity check failed, unused while parity is off |
| `uart_tx_dropped` | Bytes `uart_write` refused because the buffer was full |

On the F4 these flags clear on a read of SR followed by a read of DR. Writing to
SR does nothing, which is the part that is easy to get wrong.

These errors are receive side, which is why RX is enabled at all. With transmit
only, none of these flags can ever be raised and the handling would be
unreachable.

## How I confirmed it

Measured on 2026-11-13 with the Nucleo connected to a Mac by its ST-LINK USB
cable and nothing else attached. PA2 and PA3 reach the ST-LINK virtual COM port
through the solder bridges on the board, so that one cable carries the debug
interface and the serial port at the same time.

### Transmit

`screen /dev/tty.usbmodem1403 115200` showed `Hello from STM32` repeating
cleanly: 35 consecutive lines, no corruption, no dropped characters.

This binary contains no polling path at all. `uart_send_char` does not exist in
it, so the only code that can write to the data register is
`USART2_IRQHandler`. Clean output therefore proves the interrupt fired, the NVIC
enable took effect, and the ring buffer drained.

Reading the driver state over SWD with st-util and gdb while it ran:

```
tx_head = 18  tx_tail = 18
```

18 is the length of `Hello from STM32\r\n`. Head and tail converge after every
message, which is the ring buffer emptying completely each time rather than
slowly filling.

### Receive and error handling

I sent single bytes into PA3 from the Mac at a matching and at a mismatched
baud, reading the counters over SWD after each one.

| Sent | Baud | framing | last_rx |
|---|---|---|---|
| 0x55 | 115200, matching | 0 | 0x55 |
| 0x55 | 9600, mismatched | 5 | 0x00 |
| ABCDE | 115200, matching | 0 | 0x45 |

The controls are what make this meaningful. Errors appear only when I inject a
mismatch and never otherwise. A matching byte lands in `uart_last_rx` intact. A
mismatched byte raises framing and is not stored, so the driver does not hand
corrupt data off as received. `ABCDE` leaves 0x45, which is `E`, so every byte
was serviced, and overrun stayed at 0 because each was read before the next
arrived.

One byte at the wrong baud raises five framing errors rather than one. That is
expected rather than a fault. A 9600 frame lasts about 1.04 ms, which spans
roughly 120 bit periods at 115200, so the receiver chops one slow byte into
several mis-framed ones.

### What I did not measure

I did not put a logic analyzer on PA2, so there is no measured bit period and no
capture saved here. The expected value is 8.69 microseconds, from BRR 139 giving
115108 baud.

The baud rate is still independently confirmed. The ST-LINK virtual COM port is
a separate receiver with its own clock, and it decoded 35 lines without
corruption, which cannot happen unless the real baud sits inside its tolerance
of 115200. A capture would add a measured number, not missing proof. I will add
it when I have the cable.

## Key registers

| Register | Address | Purpose |
|---|---|---|
| RCC_AHB1ENR | 0x40023830 | Enable the GPIOA clock |
| RCC_APB1ENR | 0x40023840 | Enable the USART2 clock |
| GPIOA_MODER | 0x40020000 | Put PA2 and PA3 in alternate function mode |
| GPIOA_AFRL | 0x40020020 | Select AF7 on PA2 and PA3 |
| USART2_BRR | 0x40004408 | Baud rate divisor, 12.4 fixed point |
| USART2_CR1 | 0x4000440C | UE, TE, RE, RXNEIE, TXEIE |
| USART2_SR | 0x40004400 | TXE, RXNE and the error flags |
| USART2_DR | 0x40004404 | Data register, reading it clears error flags |
| NVIC_ISER1 | 0xE000E104 | Enable interrupt 38 |

## What I learned

- BRR is a fixed point divisor, not a pair of fields to pack by hand. Getting
  that wrong cost me a session of garbled output.
- The clock a peripheral actually runs from is whatever the reset state gives
  you until you configure RCC. Assuming 42 MHz because APB1 can reach it was the
  root of my mistake.
- Moving transmission into an interrupt is less about speed than about not
  owning the CPU while the hardware shifts bits out.
- Error flags on the F4 clear through a read sequence, not by writing to the
  status register.
- Framing and overrun only exist if the receiver is on, so a transmit only
  driver cannot honestly claim to handle them.
