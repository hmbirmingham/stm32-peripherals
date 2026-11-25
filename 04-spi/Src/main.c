#include <stdint.h>
#include "../Inc/bme280_comp.h"
#include "../Inc/ccr.h"

/* ------------------------------------------------------------------ */
/* RCC                                                                  */
/* ------------------------------------------------------------------ */
#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830)
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840)
#define RCC_APB2ENR  (*(volatile uint32_t *)0x40023844)

#define RCC_AHB1ENR_GPIOAEN  (1u << 0)
#define RCC_AHB1ENR_GPIOBEN  (1u << 1)
#define RCC_APB1ENR_I2C1EN   (1u << 21)
#define RCC_APB2ENR_SPI1EN   (1u << 12)


/* ------------------------------------------------------------------ */
/* GPIOA  —  PA8 driven HIGH to hold CSB=1 (I2C mode) on the BME280  */
/* in case that wire is still plugged into D7 instead of the VCC rail */
/* ------------------------------------------------------------------ */
#define GPIOA_MODER   (*(volatile uint32_t *)0x40020000)
#define GPIOA_ODR     (*(volatile uint32_t *)0x40020014)

/* ------------------------------------------------------------------ */
/* GPIOB  —  PB8=I2C1_SCL  PB9=I2C1_SDA, both AF4, open-drain        */
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

#define I2C_CR1_PE     (1u << 0)
#define I2C_CR1_START  (1u << 8)
#define I2C_CR1_STOP   (1u << 9)
#define I2C_CR1_ACK    (1u << 10)
#define I2C_CR1_SWRST  (1u << 15)

#define I2C_SR1_SB    (1u << 0)
#define I2C_SR1_ADDR  (1u << 1)
#define I2C_SR1_BTF   (1u << 2)
#define I2C_SR1_RXNE  (1u << 6)
#define I2C_SR1_TXE   (1u << 7)
#define I2C_SR1_BERR  (1u << 8)
#define I2C_SR1_ARLO  (1u << 9)
#define I2C_SR1_AF    (1u << 10)

#define I2C_SR2_BUSY  (1u << 1)

/* ------------------------------------------------------------------ */
/* SSD1306 OLED on I2C1                                                */
/* ------------------------------------------------------------------ */
#define SSD1306_ADDR  0x3Cu
#define SSD1306_CMD   0x00u
#define SSD1306_DATA  0x40u

/* ------------------------------------------------------------------ */
/* BME280                                                               */
/* ------------------------------------------------------------------ */
#define BME280_REG_ID       0xD0u
#define BME280_REG_RESET    0xE0u
#define BME280_REG_CTRL_HUM 0xF2u
#define BME280_REG_STATUS   0xF3u
#define BME280_REG_CTRL_MSR 0xF4u
#define BME280_REG_CONFIG   0xF5u
#define BME280_REG_PRESS_H  0xF7u
#define BME280_REG_CALIB_TP 0x88u  /* T1..P9, 24 bytes */
#define BME280_REG_CALIB_H1 0xA1u
#define BME280_REG_CALIB_H2 0xE1u  /* H2..H6, 7 bytes */

#define BME280_CHIP_ID   0x60u
static uint8_t bme280_addr = 0x76u;  /* updated by bme280_init after probing */

/* ------------------------------------------------------------------ */
/* Clocks and timeouts                                                  */
/* ------------------------------------------------------------------ */
#define PCLK1_HZ    16000000u
#define I2C_TIMEOUT 100000u
#define SPI_TIMEOUT 100000u

/* ------------------------------------------------------------------ */
/* Debugger-visible state                                               */
/* ------------------------------------------------------------------ */
volatile uint32_t i2c_err_bus;
volatile uint32_t i2c_err_timeout;
volatile uint8_t  ssd1306_found_addr;
volatile uint8_t  bme280_found;

/* ------------------------------------------------------------------ */
/* 5×8 font (ASCII 0x20–0x7E)                                          */
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
/* Shared helpers                                                       */
/* ------------------------------------------------------------------ */
static void delay(volatile uint32_t n) { while (n--); }

/* ------------------------------------------------------------------ */
/* BME280 I2C transport                                                 */
/*                                                                      */
/* The module's SDO pin is tied to GND, so the I2C address is 0x76.   */
/* Both i2c_write and i2c_recv are defined after the I2C peripheral    */
/* driver below; forward declarations let bme280_read/write call them. */
/* ------------------------------------------------------------------ */
static int  i2c_write(uint8_t addr7, const uint8_t *data, uint32_t len);
static int  i2c_recv(uint8_t addr7, uint8_t *buf, uint32_t len);

static void bme280_read(uint8_t reg, uint8_t *buf, uint32_t n)
{
    uint8_t r = reg;
    i2c_write(bme280_addr, &r, 1u);
    i2c_recv(bme280_addr, buf, n);
}

static void bme280_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    i2c_write(bme280_addr, buf, 2u);
}

/* ------------------------------------------------------------------ */
/* BME280 calibration load and measurement                             */
/* ------------------------------------------------------------------ */

/* Parse the 24-byte T/P calibration block (little-endian). */
static void bme280_load_calib_tp(bme280_calib_t *c, const uint8_t *b)
{
    c->dig_T1 = (uint16_t)(b[1]  << 8u) | b[0];
    c->dig_T2 = (int16_t) ((b[3]  << 8u) | b[2]);
    c->dig_T3 = (int16_t) ((b[5]  << 8u) | b[4]);
    c->dig_P1 = (uint16_t)(b[7]  << 8u) | b[6];
    c->dig_P2 = (int16_t) ((b[9]  << 8u) | b[8]);
    c->dig_P3 = (int16_t) ((b[11] << 8u) | b[10]);
    c->dig_P4 = (int16_t) ((b[13] << 8u) | b[12]);
    c->dig_P5 = (int16_t) ((b[15] << 8u) | b[14]);
    c->dig_P6 = (int16_t) ((b[17] << 8u) | b[16]);
    c->dig_P7 = (int16_t) ((b[19] << 8u) | b[18]);
    c->dig_P8 = (int16_t) ((b[21] << 8u) | b[20]);
    c->dig_P9 = (int16_t) ((b[23] << 8u) | b[22]);
}

/* Parse the 7-byte humidity calibration block (0xE1-0xE7).
 * dig_H4 and dig_H5 share byte 0xE5 with unusual bit packing. */
static void bme280_load_calib_h(bme280_calib_t *c, uint8_t h1, const uint8_t *e)
{
    c->dig_H1 = h1;
    c->dig_H2 = (int16_t)((e[1] << 8u) | e[0]);
    c->dig_H3 = e[2];
    /* dig_H4 = [11:4] from e[3], [3:0] from e[4][3:0] */
    c->dig_H4 = (int16_t)(((int16_t)e[3] << 4u) | (e[4] & 0x0Fu));
    /* dig_H5 = [11:4] from e[5], [3:0] from e[4][7:4] */
    c->dig_H5 = (int16_t)(((int16_t)e[5] << 4u) | (e[4] >> 4u));
    c->dig_H6 = (int8_t)e[6];
}

/* Initialise BME280: probe chip ID, load calibration, set normal mode.
 * Returns 0 on success, -1 if the chip does not respond. */
int bme280_init(bme280_calib_t *c)
{
    /* Probe 0x76 then 0x77 — SDO low = 0x76, SDO high/float = 0x77 */
    uint8_t id = 0, r = BME280_REG_ID;
    const uint8_t candidates[2] = {0x76u, 0x77u};
    uint8_t found = 0;
    for (uint8_t i = 0; i < 2u; i++) {
        if (i2c_write(candidates[i], &r, 1u) == 0) {
            i2c_recv(candidates[i], &id, 1u);
            if (id == BME280_CHIP_ID) { bme280_addr = candidates[i]; found = 1u; break; }
        }
    }
    if (!found) return -1;

    /* Load T/P calibration (24 bytes starting at 0x88). */
    uint8_t tp_buf[24];
    bme280_read(BME280_REG_CALIB_TP, tp_buf, 24u);
    bme280_load_calib_tp(c, tp_buf);

    /* Load H1 (single byte at 0xA1) and H2-H6 (7 bytes at 0xE1). */
    uint8_t h1;
    bme280_read(BME280_REG_CALIB_H1, &h1, 1u);
    uint8_t h_buf[7];
    bme280_read(BME280_REG_CALIB_H2, h_buf, 7u);
    bme280_load_calib_h(c, h1, h_buf);

    /* ctrl_hum must be written before ctrl_meas takes effect (datasheet §5.4.3).
     * osrs_h = 001 → 1× humidity oversampling. */
    bme280_write(BME280_REG_CTRL_HUM, 0x01u);

    /* osrs_t=001 (1×), osrs_p=001 (1×), mode=11 (normal). */
    bme280_write(BME280_REG_CTRL_MSR, (0x1u << 5u) | (0x1u << 2u) | 0x3u);

    return 0;
}

/* Read one set of raw measurements and return compensated values.
 * t_01C: temperature in 0.01 °C
 * p_pa256: pressure in Pa×256
 * h_q10: humidity in %RH×1024
 */
void bme280_read_all(const bme280_calib_t *c,
                     int32_t *t_01C, uint32_t *p_pa256, uint32_t *h_q10)
{
    uint8_t b[8];
    /* 0xF7-0xFE: press[2], temp[2], hum[2] in that order */
    bme280_read(BME280_REG_PRESS_H, b, 8u);

    /* Raw values: each is 20 bits (press/temp) or 16 bits (hum). */
    int32_t adc_P = ((int32_t)b[0] << 12) | ((int32_t)b[1] << 4) | (b[2] >> 4);
    int32_t adc_T = ((int32_t)b[3] << 12) | ((int32_t)b[4] << 4) | (b[5] >> 4);
    int32_t adc_H = ((int32_t)b[6] << 8)  |  b[7];

    int32_t t_fine;
    *t_01C   = bme280_comp_temp(c, adc_T, &t_fine);
    *p_pa256 = bme280_comp_press(c, adc_P, t_fine);
    *h_q10   = bme280_comp_hum(c, adc_H, t_fine);
}

/* ------------------------------------------------------------------ */
/* I2C1 driver (identical to 03-i2c)                                   */
/* ------------------------------------------------------------------ */
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

static void i2c_stop(void)
{
    I2C1_CR1 |= I2C_CR1_STOP;
    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        if (!(I2C1_SR2 & I2C_SR2_BUSY)) break;
    }
}

void i2c_init(void)
{
    /* PA8 → output HIGH: holds BME280 CSB=1 (I2C mode) if still wired to D7 */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA_MODER &= ~(0x3u << (8u * 2u));
    GPIOA_MODER |=  (0x1u << (8u * 2u));
    GPIOA_ODR   |=  (1u << 8u);

    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;

    GPIOB_MODER   &= ~((0x3u << 16u) | (0x3u << 18u));
    GPIOB_MODER   |=  ((0x2u << 16u) | (0x2u << 18u));
    GPIOB_OTYPER  |=  (1u << 8u) | (1u << 9u);
    GPIOB_OSPEEDR &= ~((0x3u << 16u) | (0x3u << 18u));
    GPIOB_OSPEEDR |=  ((0x2u << 16u) | (0x2u << 18u));
    GPIOB_AFRH    &= ~((0xFu << 0u) | (0xFu << 4u));
    GPIOB_AFRH    |=  ((0x4u << 0u) | (0x4u << 4u));

    I2C1_CR1 = I2C_CR1_SWRST;
    delay(100u);
    I2C1_CR1 = 0;

    I2C1_CR2   = i2c_cr2_freq(PCLK1_HZ);
    I2C1_CCR   = i2c_ccr_sm(PCLK1_HZ);
    I2C1_TRISE = i2c_trise_sm(PCLK1_HZ);
    I2C1_CR1  |= I2C_CR1_PE;
}

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
        if (sr1 & I2C_SR1_AF)  { I2C1_SR1 &= ~I2C_SR1_AF; i2c_stop(); return -1; }
        if (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) {
            I2C1_SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO);
            i2c_err_bus++; i2c_stop(); return -1;
        }
        if (sr1 & I2C_SR1_ADDR) break;
        if (t == I2C_TIMEOUT - 1u) { i2c_err_timeout++; return -1; }
    }
    (void)I2C1_SR1;
    (void)I2C1_SR2;
    for (uint32_t i = 0; i < len; i++) {
        if (wait_sr1(I2C_SR1_TXE) < 0) { i2c_stop(); return -1; }
        I2C1_DR = data[i];
    }
    if (len > 0u) {
        if (wait_sr1(I2C_SR1_BTF) < 0) { i2c_stop(); return -1; }
    }
    i2c_stop();
    return 0;
}

/* Receive len bytes from I2C device addr7.  Caller must have already
 * written the register address with i2c_write.  Handles both the
 * 1-byte special case and the general N-byte case (RM0383 §27.3.3). */
static int i2c_recv(uint8_t addr7, uint8_t *buf, uint32_t len)
{
    if (len == 0u) return 0;

    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        if (!(I2C1_SR2 & I2C_SR2_BUSY)) break;
    }
    I2C1_CR1 |= I2C_CR1_ACK | I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB) < 0) return -1;
    I2C1_DR = (uint32_t)((addr7 << 1u) | 1u);  /* address + read bit */

    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        uint32_t sr1 = I2C1_SR1;
        if (sr1 & I2C_SR1_AF)  { I2C1_SR1 &= ~I2C_SR1_AF; i2c_stop(); return -1; }
        if (sr1 & I2C_SR1_ADDR) break;
        if (t == I2C_TIMEOUT - 1u) { i2c_err_timeout++; return -1; }
    }

    if (len == 1u) {
        /* 1-byte: NAK+STOP before clearing ADDR, then wait RXNE */
        I2C1_CR1 &= ~I2C_CR1_ACK;
        (void)I2C1_SR1; (void)I2C1_SR2;
        I2C1_CR1 |= I2C_CR1_STOP;
        if (wait_sr1(I2C_SR1_RXNE) < 0) return -1;
        buf[0] = (uint8_t)I2C1_DR;
    } else {
        (void)I2C1_SR1; (void)I2C1_SR2;   /* clear ADDR */
        for (uint32_t i = 0u; i < len; i++) {
            if (i == len - 1u) {
                /* Last byte: clear ACK before it arrives, STOP after */
                I2C1_CR1 &= ~I2C_CR1_ACK;
                if (wait_sr1(I2C_SR1_RXNE) < 0) { i2c_stop(); return -1; }
                I2C1_CR1 |= I2C_CR1_STOP;
            } else {
                if (wait_sr1(I2C_SR1_RXNE) < 0) { i2c_stop(); return -1; }
            }
            buf[i] = (uint8_t)I2C1_DR;
        }
    }
    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        if (!(I2C1_SR2 & I2C_SR2_BUSY)) break;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* SSD1306 OLED driver                                                  */
/* ------------------------------------------------------------------ */
static void ssd1306_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {SSD1306_CMD, cmd};
    i2c_write(SSD1306_ADDR, buf, 2u);
}

static void ssd1306_set_cursor(uint8_t page, uint8_t px_col)
{
    ssd1306_cmd(0xB0u | (page & 0x07u));
    ssd1306_cmd(0x00u | (px_col & 0x0Fu));
    ssd1306_cmd(0x10u | ((px_col >> 4u) & 0x0Fu));
}

void ssd1306_init(void)
{
    delay(800000u);
    ssd1306_cmd(0xAE);
    ssd1306_cmd(0xD5); ssd1306_cmd(0x80);
    ssd1306_cmd(0xA8); ssd1306_cmd(0x3F);
    ssd1306_cmd(0xD3); ssd1306_cmd(0x00);
    ssd1306_cmd(0x40);
    ssd1306_cmd(0x8D); ssd1306_cmd(0x14);
    ssd1306_cmd(0x20); ssd1306_cmd(0x02);
    ssd1306_cmd(0xA1);
    ssd1306_cmd(0xC8);
    ssd1306_cmd(0xDA); ssd1306_cmd(0x12);
    ssd1306_cmd(0x81); ssd1306_cmd(0xCF);
    ssd1306_cmd(0xD9); ssd1306_cmd(0xF1);
    ssd1306_cmd(0xDB); ssd1306_cmd(0x40);
    ssd1306_cmd(0xA4);
    ssd1306_cmd(0xA6);
    ssd1306_cmd(0xAF);
}

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

void ssd1306_char(uint8_t page, uint8_t char_col, char c)
{
    if ((uint8_t)c < 0x20u || (uint8_t)c > 0x7Eu) c = '?';
    ssd1306_set_cursor(page, (uint8_t)(char_col * 6u));
    uint8_t buf[7];
    buf[0] = SSD1306_DATA;
    const uint8_t *glyph = font5x8[(uint8_t)c - 0x20u];
    for (uint32_t i = 0u; i < 5u; i++) buf[i + 1u] = glyph[i];
    buf[6] = 0x00u;
    i2c_write(SSD1306_ADDR, buf, 7u);
}

void ssd1306_string(uint8_t page, uint8_t start_col, const char *s)
{
    for (uint8_t col = start_col; *s != '\0'; s++, col++) {
        ssd1306_char(page, col, *s);
    }
}

/* ------------------------------------------------------------------ */
/* String formatting helpers (no libc printf)                           */
/* ------------------------------------------------------------------ */

/* Write unsigned decimal into buf (not null-terminated, returns chars written). */
static uint8_t fmt_uint(char *buf, uint32_t v)
{
    if (v == 0u) { buf[0] = '0'; return 1u; }
    char tmp[10];
    uint8_t n = 0;
    while (v > 0u) { tmp[n++] = '0' + (char)(v % 10u); v /= 10u; }
    for (uint8_t i = 0; i < n; i++) buf[i] = tmp[n - 1u - i];
    return n;
}

/* Format signed integer with implied decimal point d_places from the right.
 * e.g. fmt_fixed(buf, 2508, 2) → "25.08"
 *      fmt_fixed(buf, 1006, 1) → "100.6"
 * Returns pointer past the last character written (buf is NOT null-terminated). */
static uint8_t fmt_fixed(char *buf, int32_t v, uint8_t d_places)
{
    uint8_t pos = 0;
    if (v < 0) { buf[pos++] = '-'; v = -v; }

    /* Compute divisor = 10^d_places */
    uint32_t div = 1u;
    for (uint8_t i = 0; i < d_places; i++) div *= 10u;

    uint32_t uv = (uint32_t)v;
    pos += fmt_uint(buf + pos, uv / div);
    if (d_places > 0u) {
        buf[pos++] = '.';
        uint32_t frac = uv % div;
        /* Zero-pad the fractional part */
        for (uint8_t i = d_places - 1u; i > 0u; i--) {
            if (frac < (uint32_t)(1u << 0u)) { /* always false but keeps loop */ }
            uint32_t p = 1u;
            for (uint8_t j = 0; j < i; j++) p *= 10u;
            buf[pos++] = (char)('0' + frac / p);
            frac %= p;
        }
        buf[pos++] = (char)('0' + frac);
    }
    return pos;
}

/* Build a null-terminated line like "T: 25.08 C" into out[].
 * Caller must ensure out is large enough (22 bytes is safe). */
static void fmt_temp(char *out, int32_t t_01C)
{
    uint8_t pos = 0;
    out[pos++] = 'T'; out[pos++] = ':'; out[pos++] = ' ';
    pos += fmt_fixed(out + pos, t_01C, 2u);   /* e.g. "25.08" */
    out[pos++] = ' '; out[pos++] = 'C';
    out[pos] = '\0';
}

static void fmt_press(char *out, uint32_t p_pa256)
{
    /* Convert Pa*256 → tenths-of-hPa for one decimal place:
     * hPa_x10 = (p_pa256 / 256) / 10 = p_pa256 / 2560 */
    int32_t hpa_x10 = (int32_t)(p_pa256 / 2560u);
    uint8_t pos = 0;
    out[pos++] = 'P'; out[pos++] = ':'; out[pos++] = ' ';
    pos += fmt_fixed(out + pos, hpa_x10, 1u);
    out[pos++] = ' '; out[pos++] = 'h'; out[pos++] = 'P'; out[pos++] = 'a';
    out[pos] = '\0';
}

static void fmt_hum(char *out, uint32_t h_q10)
{
    /* Convert %RH*1024 → tenths-of-percent:
     * pct_x10 = (h_q10 * 10) / 1024 */
    int32_t pct_x10 = (int32_t)((h_q10 * 10u) / 1024u);
    uint8_t pos = 0;
    out[pos++] = 'H'; out[pos++] = ':'; out[pos++] = ' ';
    pos += fmt_fixed(out + pos, pct_x10, 1u);
    out[pos++] = ' '; out[pos++] = '%';
    out[pos] = '\0';
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void)
{
    i2c_init();

    static bme280_calib_t calib;
    if (bme280_init(&calib) == 0) {
        bme280_found = 1u;
    }

    if (i2c_write(SSD1306_ADDR, (const uint8_t[]){SSD1306_CMD}, 1u) == 0) {
        ssd1306_found_addr = SSD1306_ADDR;
    }

    ssd1306_init();
    ssd1306_clear();

    /* I2C bus scan: show every address that ACKs so we can see what's present */
    if (!bme280_found) {
        ssd1306_string(0u, 0u, "I2C scan:");
        uint8_t found_count = 0;
        char scan_line[22];
        uint8_t scan_col = 0;
        uint8_t scan_row = 2u;
        uint8_t dummy = 0;
        for (uint8_t addr = 0x08u; addr <= 0x77u; addr++) {
            if (i2c_write(addr, &dummy, 0u) == 0) {
                /* device at this address — print it */
                scan_line[0] = '0'; scan_line[1] = 'x';
                scan_line[2] = "0123456789ABCDEF"[(addr >> 4u) & 0xFu];
                scan_line[3] = "0123456789ABCDEF"[addr & 0xFu];
                scan_line[4] = ' '; scan_line[5] = '\0';
                ssd1306_string(scan_row, scan_col, scan_line);
                scan_col += 5u;
                if (scan_col >= 20u) { scan_col = 0u; scan_row += 2u; }
                found_count++;
            }
        }
        if (found_count == 0u) {
            ssd1306_string(2u, 0u, "No devices found");
        }
        for (;;);
    }

    if (0) {  /* dead branch — real not-found path below */
        /* Show the raw chip ID so we can tell if SPI is talking at all.
         * 0x60=BME280 (should pass), 0x58=BMP280, 0xFF=MISO floating,
         * 0x00=CS or clock not reaching sensor. */
        /* Try both addresses so we can see if either responds */
        uint8_t raw_id = 0, probe_r = BME280_REG_ID;
        if (i2c_write(0x76u, &probe_r, 1u) == 0) { i2c_recv(0x76u, &raw_id, 1u); }
        else if (i2c_write(0x77u, &probe_r, 1u) == 0) { i2c_recv(0x77u, &raw_id, 1u); }
        char id_line[20];
        uint8_t pos = 0;
        id_line[pos++] = 'I'; id_line[pos++] = 'D'; id_line[pos++] = ':';
        id_line[pos++] = ' '; id_line[pos++] = '0'; id_line[pos++] = 'x';
        id_line[pos++] = "0123456789ABCDEF"[(raw_id >> 4u) & 0xFu];
        id_line[pos++] = "0123456789ABCDEF"[raw_id & 0xFu];
        id_line[pos] = '\0';
        ssd1306_string(2u, 0u, "BME280 not found");
        ssd1306_string(4u, 0u, id_line);
        for (;;);
    }

    ssd1306_string(0u, 3u, "STM32 04-SPI");

    char line[22];

    for (;;) {
        int32_t  t_01C;
        uint32_t p_pa256;
        uint32_t h_q10;
        bme280_read_all(&calib, &t_01C, &p_pa256, &h_q10);

        fmt_temp(line, t_01C);
        ssd1306_string(2u, 0u, "              ");  /* clear line */
        ssd1306_string(2u, 0u, line);

        fmt_press(line, p_pa256);
        ssd1306_string(4u, 0u, "              ");
        ssd1306_string(4u, 0u, line);

        fmt_hum(line, h_q10);
        ssd1306_string(6u, 0u, "              ");
        ssd1306_string(6u, 0u, line);

        delay(2000000u);  /* ~250 ms between screen updates */
    }
}
