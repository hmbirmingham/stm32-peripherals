# 03-i2c — I2C1 master driver and SSD1306 OLED display

Bare-metal I2C1 master driver for the STM32F411RE Nucleo board. Polling,
register-level, no HAL. Drives a 128×64 SSD1306 OLED display over PB8 (SCL)
and PB9 (SDA) at 100 kHz standard mode.

## Hardware

- STM32 Nucleo-F411RE
- SSD1306 128×64 OLED breakout (I2C address 0x3C, or 0x3D if SA0 is high)
- 4.7 kΩ pull-up resistors on SDA and SCL to 3.3 V, unless the breakout
  already has them (check for R1/R2 near the I2C header pins)

**Connections:**

| Nucleo pin | OLED breakout |
|---|---|
| PB8 (D15) | SCL |
| PB9 (D14) | SDA |
| 3V3        | VCC |
| GND        | GND |

## Demo

After reset the display shows:

```
I2C on STM32
Peripheral 03
```

centered on a black background. The green LED on PA5 blinks as a heartbeat.

Before init the driver probes both 0x3C and 0x3D. The found address is
visible as `ssd1306_found_addr` in the debugger. If it stays 0, check wiring
and pull-ups.

## Build and flash

```
cd 03-i2c
make
make flash
```

## Host test (clock math, no board needed)

```
cd 03-i2c
make test
```

Verifies the CCR, TRISE, and CR2 FREQ formulas against known values for
16 MHz, 42 MHz, and 8 MHz peripheral clocks.

## Bench confirmation checklist

- [ ] `ssd1306_found_addr == 0x3C` (or 0x3D) in debugger before init
- [ ] Display shows "I2C on STM32" and "Peripheral 03" after reset
- [ ] `i2c_err_bus == 0` and `i2c_err_timeout == 0` in debugger
- [ ] LED heartbeat blinking on PA5
- [ ] Logic analyzer confirms 100 kHz SCL on PB8 (optional)
