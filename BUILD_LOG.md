# Build log

Decisions and tradeoffs from building the drivers in this repository. This repo
ends at UART. My next work is a separate repository for a systolic array and
3D sensing, so I2C, SPI, ADC and FreeRTOS are not planned here.

## Decisions

### No HAL, no CMSIS, no vendor headers

I wrote every register access by hand from the reference manual. The cost is
that I have to look up every address and bit position myself and there is
nothing to catch a typo. The benefit is that I now know what the generated code
would have been doing, which was the entire point.

### Register access through macros rather than struct overlays

Both work. I used macros of the form
`#define USART2_SR (*(volatile uint32_t *)0x40004400)` because each address is
explicit at the point of use and matches the reference manual tables directly.
A struct overlay is tidier and gives one base pointer per peripheral, but it
relies on the field order and padding matching the hardware exactly, and a
mistake there is silent. I had both styles in the repo at one point and settled
on macros for consistency.

### Left the chip on the 16 MHz HSI

Nothing in either project writes RCC_CFGR, so the F411 runs from the 16 MHz
internal oscillator it wakes up on. Configuring the PLL to reach 100 MHz would
have meant a flash latency change and a voltage scaling decision, neither of
which has anything to do with the peripherals I was trying to learn.

This decision caused my worst bug. I calculated the UART baud rate divisor for
a 42 MHz APB1 clock, which is what APB1 can reach once the PLL is configured,
not what it was actually running at. The clock a peripheral runs from is
whatever the reset state gives you until you change it.

### Interrupt driven TX rather than DMA

Both satisfy the goal of keeping the main path free. I chose interrupts because
the per byte cost is irrelevant at 115200 baud and the interrupt path is about
fifteen lines against a DMA stream and channel setup I would have had to look up
and could not verify as easily. DMA is the right answer at high throughput or
for large blocks, and it is what I would reach for in the new repo.

### Drop on a full buffer rather than block

`uart_write` returns the number of bytes it accepted and counts the rest in
`uart_tx_dropped`. Blocking until space frees up would reintroduce exactly the
stall I moved transmission into the ISR to avoid. The caller gets a count and
can decide. The tradeoff is that a caller which ignores the return value loses
data silently, which is why the counter exists.

### Power of two buffer size

64 bytes, so the wrap is a bitwise mask instead of a modulo. On a Cortex-M4 the
division would be fine, but the mask is free and the constraint costs nothing.

### No critical section around the ring indices

Only `main` writes the head index and only the ISR writes the tail. Each side
reads the other's index once per pass. With both declared volatile on a single
core part that is safe without disabling interrupts. This is the assumption most
likely to bite someone later: the moment a second producer appears it needs a
`__disable_irq` pair around the head update.

### Enabled the receiver to make the error handling real

Framing and overrun are receive side conditions. A transmit only driver can
never raise those flags, so error handling written against them would be
unreachable code that I would then be describing in a README as working. I
enabled RX on PA3 instead, which makes the errors reachable and lets me trigger
a framing error deliberately by sending at the wrong baud.

### Vendored the ST generated sources

`startup_stm32f411retx.s`, `syscalls.c` and `sysmem.c` are marked
`linguist-vendored` in `.gitattributes`. That is roughly 49 KB of code ST
generated, against about 2 KB I wrote. Leaving it counted meant the repository
language breakdown described someone else's work as mine.

## Mistakes worth recording

### The baud rate divisor

I wrote `USART2_BRR = (138 << 4) | 14`, having read that BRR holds a mantissa
and a fraction. It does, but the register value already is the fixed point
number: the low four bits are the fraction. Shifting the integer part left by
four multiplies by sixteen a second time. The register held 0x8AE, or 2222,
which is about 7200 baud. Everything came out garbled and I assumed the wiring.

The fix is that BRR equals the clock divided by the baud rate, rounded. That is
now a function in `Inc/brr.h` with a host test, including a regression case that
fails if the old value ever comes back.

### Clearing error flags

The USART error flags on the F4 do not clear by writing to the status register.
They clear on a read of SR followed by a read of DR. The ISR reads DR and
discards the value for exactly that reason.

## Later

Things I noticed while closing this repository out and chose not to fix here.

- No continuous integration. The BRR test runs on the host with no board, so a
  workflow that compiles and runs it on every push would be about fifteen lines
  and would keep the one piece of verified logic verified.
- The delay in `main` is still a busy wait on a volatile counter. It is not a
  blocking UART call, so the no blocking requirement is met, but a SysTick
  comparison would be honest about it.
- The ring buffer is not host testable because it is entangled with the register
  macros. Extracting it behind a small interface would make it testable without
  a board. That belongs in the new repo rather than as a refactor here.
- The GitHub language breakdown may need a push to the default branch before it
  recalculates. If assembly is still showing after this work lands, the next
  lever is `linguist-generated` on the startup files, which also hides them from
  diffs.
- `main` was not protected. Eight commits reached it directly rather than
  through the branch flow, which is how the integration branch ended up behind
  the default branch. Worth setting up on day one of the next repository.
- The `session-1-uart-baud` branch is stale and merged nowhere. It can be
  deleted once UART is confirmed.
- No TC interrupt. TXE fires when the shift register accepts a byte, not when
  the last bit has left the pin. That distinction only matters for something
  like releasing an RS-485 driver enable, which this does not do.
- `uart_last_rx` holds only the most recent received byte. There is no receive
  ring buffer, because nothing in this project consumes received data.

## Outstanding

UART is not confirmed on hardware. The driver compiles clean and the baud
calculation is tested on the host, but the interrupt path has never executed.
Confirming it means flashing the board, checking the serial output, capturing
PA2 on the logic analyzer at about 8.69 microseconds per bit, and deliberately
triggering a framing error to show the counters move. The procedure is written
down in `02-uart/README.md`. Until that is done the status stays In Progress and
there is no release tag.
