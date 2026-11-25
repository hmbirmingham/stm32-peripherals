#ifndef BME280_COMP_H
#define BME280_COMP_H

#include <stdint.h>

/*
 * BME280 trimming parameters, stored in NVM on the sensor.
 * Names match the datasheet exactly so the compensation code is easy to
 * cross-check against the reference formulas in section 4.2.3.
 */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;

    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
} bme280_calib_t;

/*
 * bme280_comp_temp
 *
 * Returns temperature in units of 0.01 degC.  Also writes t_fine, which
 * the pressure and humidity compensations need; call temp first.
 *
 * adc_T: raw 20-bit ADC value from registers 0xFA–0xFC (bits 19:4 from
 *        the three bytes, left-aligned, then >>4 per the datasheet).
 */
static inline int32_t bme280_comp_temp(const bme280_calib_t *c,
                                       int32_t adc_T,
                                       int32_t *t_fine)
{
    int32_t var1 = ((adc_T >> 3) - ((int32_t)c->dig_T1 << 1));
    var1 = (var1 * (int32_t)c->dig_T2) >> 11;

    int32_t var2 = (adc_T >> 4) - (int32_t)c->dig_T1;
    var2 = (((var2 * var2) >> 12) * (int32_t)c->dig_T3) >> 14;

    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) >> 8;   /* units: 0.01 degC */
}

/*
 * bme280_comp_press
 *
 * Returns pressure in units of Pa * 256 (Q24.8 fixed-point).
 * Divide the result by 256 to get whole pascals, or by 25600 for hPa.
 *
 * Call bme280_comp_temp first to get t_fine.
 */
static inline uint32_t bme280_comp_press(const bme280_calib_t *c,
                                         int32_t adc_P,
                                         int32_t t_fine)
{
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)c->dig_P6;
    var2 += (var1 * (int64_t)c->dig_P5) << 17;
    var2 += (int64_t)c->dig_P4 << 35;
    var1  = ((var1 * var1 * (int64_t)c->dig_P3) >> 8)
           + ((var1 * (int64_t)c->dig_P2) << 12);
    var1  = (((int64_t)1 << 47) + var1) * (int64_t)c->dig_P1 >> 33;

    if (var1 == 0) return 0;    /* avoid division by zero */

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)c->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)c->dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)c->dig_P7 << 4);
    return (uint32_t)p;         /* units: Pa * 256 */
}

/*
 * bme280_comp_hum
 *
 * Returns relative humidity in units of 1/1024 %RH (Q22.10 fixed-point).
 * Divide by 1024 to get whole percent.
 *
 * Call bme280_comp_temp first to get t_fine.
 */
static inline uint32_t bme280_comp_hum(const bme280_calib_t *c,
                                       int32_t adc_H,
                                       int32_t t_fine)
{
    /* v1 = t_fine - 76800 is used throughout the right-hand sub-expression;
     * v_a is the left multiplier computed first, then reused for the final
     * product.  Keeping them separate avoids the step-1/step-2 aliasing
     * problem that appears when this is collapsed into one compound expr. */
    int32_t v1 = t_fine - 76800;

    int32_t v_a = (((adc_H << 14) - ((int32_t)c->dig_H4 << 20)
                    - ((int32_t)c->dig_H5 * v1)) + 16384) >> 15;

    int32_t inner = (((int32_t)c->dig_H6 * v1) >> 10)
                    * ((((int32_t)c->dig_H3 * v1) >> 11) + 32768);
    inner >>= 10;

    int32_t v_b = (((inner + 2097152) * (int32_t)c->dig_H2) + 8192) >> 14;

    int32_t v = v_a * v_b;
    v -= ((((v >> 15) * (v >> 15)) >> 7) * (int32_t)c->dig_H1) >> 4;
    if (v < 0)         v = 0;
    if (v > 419430400) v = 419430400;
    return (uint32_t)(v >> 12);  /* units: %RH * 1024 */
}

#endif /* BME280_COMP_H */
