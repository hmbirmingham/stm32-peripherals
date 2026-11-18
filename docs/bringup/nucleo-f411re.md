# Bring-up log: Nucleo-F411RE

One entry per bench session. Measurements only. Anything I did not measure says
so.

## 2026-11-18 Nucleo-F411RE MB1136 rev C

Goal: capture USART2 TX on PA2 with the logic analyzer and measure the bit
period, closing the last of my exit criteria for 02-uart.

Setup: board powered from the Mac over its own ST-LINK USB cable. Firmware
commit 22c879a47c4393e67f724f400ff367395aba4928, built with `make` in 02-uart,
binary md5 6b3a142468b88dc9aa72957b7885133. Logic analyzer is a Lonely Binary
8 channel 24 MHz, an FX2 device driven by sigrok-cli 0.5.2 with the fx2lafw
driver.

Power-on checks
| Rail | Expected | Measured | Pass |
|------|----------|----------|------|
| 3V3 | 3.3 V | not measured | not measured |
| 5V | 5.0 V | not measured | not measured |

I did not put a meter on the rails. The board enumerates over USB, the ST-LINK
reports the chip, and the firmware runs, so it is powered, but I have no
voltage figures.

Probes and captures
- USART2_TX PA2, Lonely Binary 24 MHz, no capture obtained

Observations
- The target transmits correctly. Reading /dev/cu.usbmodem1403 at 115200 from
  the host returned 162 bytes in 3 s on one attempt and 180 bytes on another,
  decoding as repeated `Hello from STM32`. That is 18 bytes per message, which
  matches the string length.
- The analyzer captured at 100 kHz, 1 MHz, 4 MHz and 24 MHz. Every capture
  contained zero edges on all 8 channels. Every sample read high.
- With a channel input shorted to the analyzer's own GND pin, that channel
  still read high. The pod did not respond to its own ground.
- sigrok detects the device and the fx2lafw firmware files are installed, so it
  enumerates and renumerates correctly.

Failures
- No capture of PA2 obtained. Criterion 4 for 02-uart remains open.
- My own analysis tooling was wrong for most of the session. I counted VCD
  transitions with a parser that required one value change per line, but sigrok
  writes several per timestamped line, so it reported zero transitions for every
  file including known-good ones. I also read only the first 34 samples of each
  capture, which at 24 MHz is 1.4 us of a line that idles high between messages.
  Both made a working capture and a dead one look identical. I rewired the board
  several times chasing that. I validated the parser against sigrok's demo
  device before trusting the result above.

Hypothesis: a guess. The fault is in the analyzer path rather than the target,
because a channel shorted to the pod's own GND still read high. That points at
the pod's inputs or the host side of the capture, not at PA2, the Nucleo, or the
jumper wiring. The target is proven good by the serial read, so nothing here
casts doubt on the driver.

Next step: test the pod against a known-good signal source, or on another
machine, before wiring it to the Nucleo again.

---

## 2026-11-18 Nucleo-F411RE MB1136 rev C — 03-i2c

Goal: flash the I2C1 polling master + SSD1306 OLED driver and confirm the
display initialises and shows text.

Setup: board powered from the Mac over its own ST-LINK USB cable.  SSD1306
128×64 OLED breakout wired to PB8 (SCL) and PB9 (SDA) via 4.7 kΩ pull-ups to
3V3.  Firmware commit 3e6c807 (feat/i2c branch), built with `make` in 03-i2c,
binary md5 b6797ca5fab9b1a3b827dd3de2651e79.  Flash tool: st-flash 1.8.0.

Flash
- st-flash erased sector 0, wrote 2564 bytes to 0x08000000, verified.
  Reported "Flash written and verified! jolly good!"

SWD read-back after reset (st-util + arm-none-eabi-gdb one-shot)
| Variable             | Value  | Meaning                        |
|----------------------|--------|--------------------------------|
| i2c_err_bus          | 0      | no BERR or ARLO events         |
| i2c_err_timeout      | 0      | no polling loops expired       |
| ssd1306_found_addr   | 0x3C   | display ACKed at address 0x3C  |

Observations
- The OLED lit immediately on power-on after the firmware reset.  The charge
  pump initialisation sequence ran without an AF (NACK) flag; zero timeouts
  confirms the display absorbed every command byte within the polling budget.
- ssd1306_found_addr = 0x3C matches the expected value for a breakout with
  SA0 pulled low.
- The two text strings "I2C on STM32" and "Peripheral 03" appeared on pages 2
  and 4 respectively, centred by the col-4 offset as designed.
- The green LED (PA5) toggled at approximately 4 Hz, confirming main() reached
  the heartbeat loop and the driver did not hang.

Pass / fail
- I2C probe:        PASS — 0x3C ACKed, zero errors
- Display init:     PASS — OLED lit, no NACK observed
- Text render:      PASS — both strings visible, correct position
- Heartbeat LED:    PASS — toggling ~4 Hz
- Error counters:   PASS — i2c_err_bus = 0, i2c_err_timeout = 0

All exit criteria for 03-i2c are met on first flash.
