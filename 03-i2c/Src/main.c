#include <stdint.h>
#include "../Inc/ccr.h"

/* ------------------------------------------------------------------ */
/* RCC                                                                  */
/* ------------------------------------------------------------------ */
#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830)
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840)

#define RCC_AHB1ENR_GPIOAEN  (1u << 0)
#define RCC_AHB1ENR_GPIOBEN  (1u << 1)
#define RCC_APB1ENR_I2C1EN   (1u << 21)

/* ------------------------------------------------------------------ */
/* GPIOA  --  PA5 is the green LED on the Nucleo                       */
/* ------------------------------------------------------------------ */
#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000)
#define GPIOA_ODR    (*(volatile uint32_t *)0x40020014)

/* ------------------------------------------------------------------ */
/* GPIOB  --  PB8 = I2C1_SCL, PB9 = I2C1_SDA, both on AF4            */
/* ------------------------------------------------------------------ */
#define GPIOB_MODER   (*(volatile uint32_t *)0x40020400)
#define GPIOB_OTYPER  (*(volatile uint32_t *)0x40020404)
#define GPIOB_OSPEEDR (*(volatile uint32_t *)0x40020408)
#define GPIOB_AFRH    (*(volatile uint32_t *)0x40020424)

/* ------------------------------------------------------------------ */
/* I2C1  (base 0x40005400)                                             */
/* ------------------------------------------------------------------ */
#define I2C1_CR1    (*(volatile uint32_t *)0x40005400)
#define I2C1_CR2    (*(volatile uint32_t *)0x40005404)
#define I2C1_DR     (*(volatile uint32_t *)0x40005410)
#define I2C1_SR1    (*(volatile uint32_t *)0x40005414)
#define I2C1_SR2    (*(volatile uint32_t *)0x40005418)
#define I2C1_CCR    (*(volatile uint32_t *)0x4000541C)
#define I2C1_TRISE  (*(volatile uint32_t *)0x40005420)

/* CR1 bits */
#define I2C_CR1_PE     (1u << 0)   /* peripheral enable              */
#define I2C_CR1_START  (1u << 8)   /* generate START condition       */
#define I2C_CR1_STOP   (1u << 9)   /* generate STOP condition        */
#define I2C_CR1_ACK    (1u << 10)  /* ACK enable (for receive)       */
#define I2C_CR1_SWRST  (1u << 15)  /* software reset                 */

/* SR1 bits */
#define I2C_SR1_SB    (1u << 0)   /* START sent; write addr to clear */
#define I2C_SR1_ADDR  (1u << 1)   /* address sent and ACKed          */
#define I2C_SR1_BTF   (1u << 2)   /* byte transfer finished          */
#define I2C_SR1_TXE   (1u << 7)   /* data register empty (TX ready)  */
#define I2C_SR1_BERR  (1u << 8)   /* bus error                       */
#define I2C_SR1_ARLO  (1u << 9)   /* arbitration lost                */
#define I2C_SR1_AF    (1u << 10)  /* acknowledge failure (NACK)      */

/* SR2 bits */
#define I2C_SR2_BUSY  (1u << 1)   /* bus is busy                     */

#define PCLK1_HZ    16000000u
#define I2C_TIMEOUT 100000u

/* ------------------------------------------------------------------ */
/* SSD1306 OLED                                                         */
/* Most breakouts pull SA0 low → address 0x3C. If yours is at 0x3D,   */
/* change this define.                                                  */
/* ------------------------------------------------------------------ */
#define SSD1306_ADDR  0x3Cu
#define SSD1306_CMD   0x00u   /* control byte: command follows  */
#define SSD1306_DATA  0x40u   /* control byte: pixel data follows */

/* ------------------------------------------------------------------ */
/* Debugger-visible state  --  read these in the debugger on the bench */
/* ------------------------------------------------------------------ */
volatile uint32_t i2c_err_bus;      /* BERR or ARLO events          */
volatile uint32_t i2c_err_timeout;  /* polling loops that expired   */
volatile uint8_t  ssd1306_found_addr; /* address the probe confirmed */

/* ------------------------------------------------------------------ */
/* 5x8 pixel font for ASCII 0x20 (space) through 0x7E (~)             */
/*                                                                      */
/* Each entry is 5 column bytes.  Bit 0 = top pixel, bit 6 = bottom.  */
/* A blank 6th column is added at draw time for character spacing,     */
/* giving a 6-pixel-wide cell and 21 characters per 128-pixel line.    */
/* ------------------------------------------------------------------ */
static const uint8_t font5x8[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},  /* 0x20 ' '  */
    {0x00,0x00,0x5F,0x00,0x00},  /* 0x21 '!'  */
    {0x00,0x07,0x00,0x07,0x00},  /* 0x22 '"'  */
    {0x14,0x7F,0x14,0x7F,0x14},  /* 0x23 '#'  */
    {0x24,0x2A,0x7F,0x2A,0x12},  /* 0x24 '$'  */
    {0x23,0x13,0x08,0x64,0x62},  /* 0x25 '%'  */
    {0x36,0x49,0x55,0x22,0x50},  /* 0x26 '&'  */
    {0x00,0x05,0x03,0x00,0x00},  /* 0x27 '\'' */
    {0x00,0x1C,0x22,0x41,0x00},  /* 0x28 '('  */
    {0x00,0x41,0x22,0x1C,0x00},  /* 0x29 ')'  */
    {0x14,0x08,0x3E,0x08,0x14},  /* 0x2A '*'  */
    {0x08,0x08,0x3E,0x08,0x08},  /* 0x2B '+'  */
    {0x00,0x50,0x30,0x00,0x00},  /* 0x2C ','  */
    {0x08,0x08,0x08,0x08,0x08},  /* 0x2D '-'  */
    {0x00,0x60,0x60,0x00,0x00},  /* 0x2E '.'  */
    {0x20,0x10,0x08,0x04,0x02},  /* 0x2F '/'  */
    {0x3E,0x51,0x49,0x45,0x3E},  /* 0x30 '0'  */
    {0x00,0x42,0x7F,0x40,0x00},  /* 0x31 '1'  */
    {0x42,0x61,0x51,0x49,0x46},  /* 0x32 '2'  */
    {0x21,0x41,0x45,0x4B,0x31},  /* 0x33 '3'  */
    {0x18,0x14,0x12,0x7F,0x10},  /* 0x34 '4'  */
    {0x27,0x45,0x45,0x45,0x39},  /* 0x35 '5'  */
    {0x3C,0x4A,0x49,0x49,0x30},  /* 0x36 '6'  */
    {0x01,0x71,0x09,0x05,0x03},  /* 0x37 '7'  */
    {0x36,0x49,0x49,0x49,0x36},  /* 0x38 '8'  */
    {0x06,0x49,0x49,0x52,0x3C},  /* 0x39 '9'  */
    {0x00,0x36,0x36,0x00,0x00},  /* 0x3A ':'  */
    {0x00,0x56,0x36,0x00,0x00},  /* 0x3B ';'  */
    {0x08,0x14,0x22,0x41,0x00},  /* 0x3C '<'  */
    {0x14,0x14,0x14,0x14,0x14},  /* 0x3D '='  */
    {0x00,0x41,0x22,0x14,0x08},  /* 0x3E '>'  */
    {0x02,0x01,0x51,0x09,0x06},  /* 0x3F '?'  */
    {0x32,0x49,0x79,0x41,0x3E},  /* 0x40 '@'  */
    {0x7E,0x11,0x11,0x11,0x7E},  /* 0x41 'A'  */
    {0x7F,0x49,0x49,0x49,0x36},  /* 0x42 'B'  */
    {0x3E,0x41,0x41,0x41,0x22},  /* 0x43 'C'  */
    {0x7F,0x41,0x41,0x22,0x1C},  /* 0x44 'D'  */
    {0x7F,0x49,0x49,0x49,0x41},  /* 0x45 'E'  */
    {0x7F,0x09,0x09,0x09,0x01},  /* 0x46 'F'  */
    {0x3E,0x41,0x49,0x49,0x7A},  /* 0x47 'G'  */
    {0x7F,0x08,0x08,0x08,0x7F},  /* 0x48 'H'  */
    {0x00,0x41,0x7F,0x41,0x00},  /* 0x49 'I'  */
    {0x20,0x40,0x41,0x3F,0x01},  /* 0x4A 'J'  */
    {0x7F,0x08,0x14,0x22,0x41},  /* 0x4B 'K'  */
    {0x7F,0x40,0x40,0x40,0x40},  /* 0x4C 'L'  */
    {0x7F,0x02,0x0C,0x02,0x7F},  /* 0x4D 'M'  */
    {0x7F,0x04,0x08,0x10,0x7F},  /* 0x4E 'N'  */
    {0x3E,0x41,0x41,0x41,0x3E},  /* 0x4F 'O'  */
    {0x7F,0x09,0x09,0x09,0x06},  /* 0x50 'P'  */
    {0x3E,0x41,0x51,0x21,0x5E},  /* 0x51 'Q'  */
    {0x7F,0x09,0x19,0x29,0x46},  /* 0x52 'R'  */
    {0x46,0x49,0x49,0x49,0x31},  /* 0x53 'S'  */
    {0x01,0x01,0x7F,0x01,0x01},  /* 0x54 'T'  */
    {0x3F,0x40,0x40,0x40,0x3F},  /* 0x55 'U'  */
    {0x1F,0x20,0x40,0x20,0x1F},  /* 0x56 'V'  */
    {0x3F,0x40,0x38,0x40,0x3F},  /* 0x57 'W'  */
    {0x63,0x14,0x08,0x14,0x63},  /* 0x58 'X'  */
    {0x07,0x08,0x70,0x08,0x07},  /* 0x59 'Y'  */
    {0x61,0x51,0x49,0x45,0x43},  /* 0x5A 'Z'  */
    {0x00,0x7F,0x41,0x41,0x00},  /* 0x5B '['  */
    {0x02,0x04,0x08,0x10,0x20},  /* 0x5C '\\' */
    {0x00,0x41,0x41,0x7F,0x00},  /* 0x5D ']'  */
    {0x04,0x02,0x01,0x02,0x04},  /* 0x5E '^'  */
    {0x40,0x40,0x40,0x40,0x40},  /* 0x5F '_'  */
    {0x00,0x01,0x02,0x04,0x00},  /* 0x60 '`'  */
    {0x20,0x54,0x54,0x54,0x78},  /* 0x61 'a'  */
    {0x7F,0x48,0x44,0x44,0x38},  /* 0x62 'b'  */
    {0x38,0x44,0x44,0x44,0x20},  /* 0x63 'c'  */
    {0x38,0x44,0x44,0x48,0x7F},  /* 0x64 'd'  */
    {0x38,0x54,0x54,0x54,0x18},  /* 0x65 'e'  */
    {0x08,0x7E,0x09,0x01,0x02},  /* 0x66 'f'  */
    {0x0C,0x52,0x52,0x52,0x3E},  /* 0x67 'g'  */
    {0x7F,0x08,0x04,0x04,0x78},  /* 0x68 'h'  */
    {0x00,0x44,0x7D,0x40,0x00},  /* 0x69 'i'  */
    {0x20,0x40,0x44,0x3D,0x00},  /* 0x6A 'j'  */
    {0x7F,0x10,0x28,0x44,0x00},  /* 0x6B 'k'  */
    {0x00,0x41,0x7F,0x40,0x00},  /* 0x6C 'l'  */
    {0x7C,0x04,0x18,0x04,0x78},  /* 0x6D 'm'  */
    {0x7C,0x08,0x04,0x04,0x78},  /* 0x6E 'n'  */
    {0x38,0x44,0x44,0x44,0x38},  /* 0x6F 'o'  */
    {0x7C,0x14,0x14,0x14,0x08},  /* 0x70 'p'  */
    {0x08,0x14,0x14,0x18,0x7C},  /* 0x71 'q'  */
    {0x7C,0x08,0x04,0x04,0x08},  /* 0x72 'r'  */
    {0x48,0x54,0x54,0x54,0x20},  /* 0x73 's'  */
    {0x04,0x3F,0x44,0x40,0x20},  /* 0x74 't'  */
    {0x3C,0x40,0x40,0x20,0x7C},  /* 0x75 'u'  */
    {0x1C,0x20,0x40,0x20,0x1C},  /* 0x76 'v'  */
    {0x3C,0x40,0x30,0x40,0x3C},  /* 0x77 'w'  */
    {0x44,0x28,0x10,0x28,0x44},  /* 0x78 'x'  */
    {0x0C,0x50,0x50,0x50,0x3C},  /* 0x79 'y'  */
    {0x44,0x64,0x54,0x4C,0x44},  /* 0x7A 'z'  */
    {0x00,0x08,0x36,0x41,0x00},  /* 0x7B '{'  */
    {0x00,0x00,0x7F,0x00,0x00},  /* 0x7C '|'  */
    {0x00,0x41,0x36,0x08,0x00},  /* 0x7D '}'  */
    {0x10,0x08,0x08,0x10,0x08},  /* 0x7E '~'  */
};

/* ------------------------------------------------------------------ */
/* Low-level helpers                                                    */
/* ------------------------------------------------------------------ */

static void delay(volatile uint32_t n)
{
    while (n--);
}

/* Wait for a flag in SR1.  Returns 0 on success, -1 on bus error or
 * timeout.  Also clears BERR/ARLO so the peripheral can recover. */
static int wait_sr1(uint32_t flag)
{
    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        uint32_t sr1 = I2C1_SR1;
        if (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) {
            I2C1_SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO);
            i2c_err_bus++;
            return -1;
        }
        if (sr1 & flag) return 0;
    }
    i2c_err_timeout++;
    return -1;
}

/* Issue STOP and wait for the bus to go idle. */
static void i2c_stop(void)
{
    I2C1_CR1 |= I2C_CR1_STOP;
    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        if (!(I2C1_SR2 & I2C_SR2_BUSY)) break;
    }
}

/* ------------------------------------------------------------------ */
/* I2C1 init                                                            */
/* ------------------------------------------------------------------ */
void i2c_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* PB8 (SCL) and PB9 (SDA):
     *   MODER  = 10 (alternate function)
     *   OTYPER = 1  (open-drain — I2C lines can only be pulled low by the
     *                chip; the external resistors pull them high)
     *   OSPEEDR = 10 (fast speed, needed at 100 kHz)
     *   AFRH[3:0] = 4 (AF4 = I2C1 for PB8)
     *   AFRH[7:4] = 4 (AF4 = I2C1 for PB9)
     * PUPDR is left at reset (no pull) because we have external resistors. */
    GPIOB_MODER   &= ~((0x3u << 16u) | (0x3u << 18u));
    GPIOB_MODER   |=  ((0x2u << 16u) | (0x2u << 18u));
    GPIOB_OTYPER  |=  (1u << 8u) | (1u << 9u);
    GPIOB_OSPEEDR &= ~((0x3u << 16u) | (0x3u << 18u));
    GPIOB_OSPEEDR |=  ((0x2u << 16u) | (0x2u << 18u));
    GPIOB_AFRH    &= ~((0xFu << 0u) | (0xFu << 4u));
    GPIOB_AFRH    |=  ((0x4u << 0u) | (0x4u << 4u));

    /* SWRST clears any stuck bus state from a previous reset mid-transfer.
     * PE must be 0 before writing CCR and TRISE (hardware requirement). */
    I2C1_CR1 = I2C_CR1_SWRST;
    delay(100u);
    I2C1_CR1 = 0;

    I2C1_CR2    = i2c_cr2_freq(PCLK1_HZ);    /* FREQ = 16 (MHz)        */
    I2C1_CCR    = i2c_ccr_sm(PCLK1_HZ);      /* CCR  = 80 → 100 kHz   */
    I2C1_TRISE  = i2c_trise_sm(PCLK1_HZ);    /* TRISE = 17             */

    I2C1_CR1 |= I2C_CR1_PE;
}

/* ------------------------------------------------------------------ */
/* i2c_probe  --  returns 0 if addr7 ACKs, -1 if NACK or error        */
/* ------------------------------------------------------------------ */
int i2c_probe(uint8_t addr7)
{
    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        if (!(I2C1_SR2 & I2C_SR2_BUSY)) break;
    }

    I2C1_CR1 |= I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB) < 0) return -1;

    /* Writing the address clears SB.  Bit 0 = 0 means write direction. */
    I2C1_DR = (uint32_t)(addr7 << 1u);

    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        uint32_t sr1 = I2C1_SR1;
        if (sr1 & I2C_SR1_AF) {
            /* NACK: the address was not acknowledged.  Clear AF, send STOP. */
            I2C1_SR1 &= ~I2C_SR1_AF;
            i2c_stop();
            return -1;
        }
        if (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) {
            I2C1_SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO);
            i2c_err_bus++;
            i2c_stop();
            return -1;
        }
        if (sr1 & I2C_SR1_ADDR) {
            /* ACK received.  Clear ADDR (read SR1 then SR2), then STOP. */
            (void)I2C1_SR1;
            (void)I2C1_SR2;
            i2c_stop();
            return 0;
        }
    }
    i2c_err_timeout++;
    return -1;
}

/* ------------------------------------------------------------------ */
/* i2c_write  --  send len bytes to addr7                              */
/* ------------------------------------------------------------------ */
int i2c_write(uint8_t addr7, const uint8_t *data, uint32_t len)
{
    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        if (!(I2C1_SR2 & I2C_SR2_BUSY)) break;
    }

    I2C1_CR1 |= I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB) < 0) return -1;

    I2C1_DR = (uint32_t)(addr7 << 1u);

    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        uint32_t sr1 = I2C1_SR1;
        if (sr1 & I2C_SR1_AF) {
            I2C1_SR1 &= ~I2C_SR1_AF;
            i2c_stop();
            return -1;
        }
        if (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) {
            I2C1_SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO);
            i2c_err_bus++;
            i2c_stop();
            return -1;
        }
        if (sr1 & I2C_SR1_ADDR) break;
        if (t == I2C_TIMEOUT - 1u) {
            i2c_err_timeout++;
            return -1;
        }
    }
    /* Clear ADDR flag: must read SR1 then SR2 in sequence. */
    (void)I2C1_SR1;
    (void)I2C1_SR2;

    for (uint32_t i = 0; i < len; i++) {
        if (wait_sr1(I2C_SR1_TXE) < 0) { i2c_stop(); return -1; }
        I2C1_DR = data[i];
    }

    /* Wait for BTF (both data register and shift register empty) before
     * issuing STOP.  Without this the STOP can cut off the last byte. */
    if (len > 0u) {
        if (wait_sr1(I2C_SR1_BTF) < 0) { i2c_stop(); return -1; }
    }

    i2c_stop();
    return 0;
}

/* ------------------------------------------------------------------ */
/* SSD1306 driver                                                       */
/* ------------------------------------------------------------------ */

/* Send one command byte.  The 0x00 control byte tells the SSD1306 that
 * what follows is a command, not pixel data. */
static void ssd1306_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {SSD1306_CMD, cmd};
    i2c_write(SSD1306_ADDR, buf, 2u);
}

/* Point the display's internal cursor to (page, pixel-column).
 * The display has 8 pages (0-7), each one is 8 pixels tall.
 * Data written after this goes left-to-right along that page. */
static void ssd1306_set_cursor(uint8_t page, uint8_t px_col)
{
    ssd1306_cmd(0xB0u | (page & 0x07u));
    ssd1306_cmd(0x00u | (px_col & 0x0Fu));           /* col low nibble  */
    ssd1306_cmd(0x10u | ((px_col >> 4u) & 0x0Fu));   /* col high nibble */
}

void ssd1306_init(void)
{
    delay(800000u);  /* ~100 ms power-on stabilisation */

    ssd1306_cmd(0xAE);        /* display off (configure while dark)         */
    ssd1306_cmd(0xD5);        /* set display clock div ratio / osc freq     */
    ssd1306_cmd(0x80);        /*   ratio = 1, freq = mid                    */
    ssd1306_cmd(0xA8);        /* set multiplex ratio                        */
    ssd1306_cmd(0x3F);        /*   63 = 64 rows (128x64 panel)              */
    ssd1306_cmd(0xD3);        /* set display offset                         */
    ssd1306_cmd(0x00);        /*   0 = no vertical shift                    */
    ssd1306_cmd(0x40);        /* set start line = 0                         */
    ssd1306_cmd(0x8D);        /* charge pump (needed to drive OLED from 3.3V)*/
    ssd1306_cmd(0x14);        /*   0x14 = on, 0x10 = off                    */
    ssd1306_cmd(0x20);        /* memory addressing mode                     */
    ssd1306_cmd(0x02);        /*   0x02 = page mode (simplest for text)     */
    ssd1306_cmd(0xA1);        /* segment remap: col 127 = seg 0             */
    ssd1306_cmd(0xC8);        /* COM scan direction: remapped (flip Y)      */
    ssd1306_cmd(0xDA);        /* COM pins hardware config                   */
    ssd1306_cmd(0x12);        /*   alt config, correct for 128x64           */
    ssd1306_cmd(0x81);        /* set contrast                               */
    ssd1306_cmd(0xCF);        /*   0xCF = bright                            */
    ssd1306_cmd(0xD9);        /* pre-charge period                          */
    ssd1306_cmd(0xF1);
    ssd1306_cmd(0xDB);        /* VCOMH deselect level                       */
    ssd1306_cmd(0x40);
    ssd1306_cmd(0xA4);        /* output follows RAM (not all-pixels-on)     */
    ssd1306_cmd(0xA6);        /* normal display (not inverted)              */
    ssd1306_cmd(0xAF);        /* display on                                 */
}

/* Fill every pixel with zero (black). Sends 1024 zero bytes in 32-byte
 * chunks: 4 chunks × 32 pixels × 8 pages = 1024 bytes. */
void ssd1306_clear(void)
{
    uint8_t buf[33];
    buf[0] = SSD1306_DATA;
    for (uint32_t i = 1u; i < 33u; i++) buf[i] = 0x00u;

    for (uint8_t page = 0u; page < 8u; page++) {
        ssd1306_set_cursor(page, 0u);
        for (uint32_t chunk = 0u; chunk < 4u; chunk++) {
            i2c_write(SSD1306_ADDR, buf, 33u);
        }
    }
}

/* Draw one character at (page, char_col).  char_col 0 = leftmost cell. */
void ssd1306_char(uint8_t page, uint8_t char_col, char c)
{
    if ((uint8_t)c < 0x20u || (uint8_t)c > 0x7Eu) c = '?';

    ssd1306_set_cursor(page, (uint8_t)(char_col * 6u));

    uint8_t buf[7];
    buf[0] = SSD1306_DATA;
    const uint8_t *glyph = font5x8[(uint8_t)c - 0x20u];
    for (uint32_t i = 0u; i < 5u; i++) buf[i + 1u] = glyph[i];
    buf[6] = 0x00u;  /* blank column between characters */

    i2c_write(SSD1306_ADDR, buf, 7u);
}

/* Draw a null-terminated string starting at (page, start_col). */
void ssd1306_string(uint8_t page, uint8_t start_col, const char *s)
{
    for (uint8_t col = start_col; *s != '\0'; s++, col++) {
        ssd1306_char(page, col, *s);
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void)
{
    /* Green LED on PA5: output mode */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA_MODER &= ~(0x3u << (5u * 2u));
    GPIOA_MODER |=  (0x1u << (5u * 2u));

    i2c_init();

    /* Probe 0x3C first, then 0x3D.  Most SSD1306 breakouts are at 0x3C.
     * ssd1306_found_addr stays 0 if nothing responds -- check wiring. */
    if (i2c_probe(0x3Cu) == 0) {
        ssd1306_found_addr = 0x3Cu;
    } else if (i2c_probe(0x3Du) == 0) {
        ssd1306_found_addr = 0x3Du;
    }

    ssd1306_init();
    ssd1306_clear();

    /* col 4 = 24 px from the left, centres 12-char strings on 128 px */
    ssd1306_string(2u, 4u, "I2C on STM32");
    ssd1306_string(4u, 4u, "Peripheral 03");

    for (;;) {
        GPIOA_ODR ^= (1u << 5u);  /* heartbeat: LED toggles ~4 Hz */
        delay(500000u);
    }
}
