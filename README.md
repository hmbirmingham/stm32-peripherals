# stm32-peripherals

Bare-metal STM32F411RE peripheral drivers written from scratch using direct register access. No HAL, no libraries.

## Projects

| Folder | Peripheral | Status |
|---|---|---|
| 01-blinky | GPIO — register-level LED toggle on PA5 | ✅ Done |
| 02-uart | USART2 — TX driver, 115200 baud | ✅ Done |
| 03-i2c-bme280 | I2C — BME280 sensor driver | 🔜 Next |
| 04-spi-ssd1306 | SPI — SSD1306 display driver | 🔜 Upcoming |
| 05-adc-dma | ADC + DMA — circular buffer sampling | 🔜 Upcoming |
| 06-freertos | FreeRTOS — 3 tasks, queues, semaphore | 🔜 Upcoming |

## Hardware
- STM32 Nucleo-F411RE
- Logic analyzer for bus verification
- CP2102 USB-UART adapter for serial monitoring
- Flashed via st-flash over Raspberry Pi

## Toolchain
ARM GCC, STM32CubeIDE, st-flash, macOS
