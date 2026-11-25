#ifndef CCR_H
#define CCR_H

#include <stdint.h>

/*
 * CR2 FREQ field: the peripheral clock in MHz.
 * The hardware uses this to time the I2C setup/hold windows.
 * Must be written before CCR and TRISE.
 */
static inline uint32_t i2c_cr2_freq(uint32_t pclk1_hz)
{
    return pclk1_hz / 1000000u;
}

/*
 * Standard-mode (100 kHz) clock control register.
 * With DUTY=0 (default), Thigh = Tlow = CCR * TPCLK1.
 * SCL = PCLK1 / (2 * CCR), so CCR = PCLK1 / 200000.
 * At 16 MHz HSI: CCR = 80.
 */
static inline uint32_t i2c_ccr_sm(uint32_t pclk1_hz)
{
    return pclk1_hz / 200000u;
}

/*
 * Maximum rise time for standard mode is 1000 ns.
 * TRISE = floor(T_rise_max / TPCLK1) + 1 = PCLK1_MHz + 1.
 * At 16 MHz HSI: TRISE = 17.
 */
static inline uint32_t i2c_trise_sm(uint32_t pclk1_hz)
{
    return pclk1_hz / 1000000u + 1u;
}

#endif
