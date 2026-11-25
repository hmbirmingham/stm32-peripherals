/*
 * Host test for the BME280 compensation formulas.
 *
 * Uses the reference calibration data and ADC values from the BME280
 * datasheet (section 4.2.3, Table 2).  Temperature has an exact expected
 * value from the datasheet (25.08 degC).  Pressure and humidity are
 * checked against physical bounds so we can catch overflow or
 * divide-by-zero without needing a second independent reference.
 *
 * Run with: make test  (from 04-spi/)
 */
#include <stdio.h>
#include <stdint.h>
#include "../Inc/bme280_comp.h"

/* Calibration data from datasheet Table 2. */
static const bme280_calib_t ref_calib = {
    .dig_T1 = 27504,
    .dig_T2 = 26435,
    .dig_T3 = -1000,

    .dig_P1 = 36477,
    .dig_P2 = -10685,
    .dig_P3 = 3024,
    .dig_P4 = 2855,
    .dig_P5 = 140,
    .dig_P6 = -7,
    .dig_P7 = 15500,
    .dig_P8 = -14600,
    .dig_P9 = 6000,

    .dig_H1 = 75,
    .dig_H2 = 370,
    .dig_H3 = 0,
    .dig_H4 = 312,
    .dig_H5 = 50,
    .dig_H6 = 30,
};

/* Raw ADC values from the same table. */
#define ADC_T  519888
#define ADC_P  415148
#define ADC_H  29812

static int fail;

static void check_exact(const char *name, int32_t got, int32_t expect)
{
    if (got == expect) {
        printf("PASS  %-8s  %d\n", name, got);
    } else {
        printf("FAIL  %-8s  got %d  expect %d\n", name, got, expect);
        fail = 1;
    }
}

/* Physical-bounds check: makes sure the formula did not overflow or
 * divide by zero.  lo and hi are in the formula's output units. */
static void check_range(const char *name, uint32_t got,
                        uint32_t lo, uint32_t hi,
                        const char *unit)
{
    if (got >= lo && got <= hi) {
        printf("PASS  %-8s  %u %s\n", name, got, unit);
    } else {
        printf("FAIL  %-8s  got %u %s  expected [%u, %u]\n",
               name, got, unit, lo, hi);
        fail = 1;
    }
}

int main(void)
{
    int32_t t_fine;
    int32_t  T = bme280_comp_temp(&ref_calib, ADC_T, &t_fine);
    uint32_t P = bme280_comp_press(&ref_calib, ADC_P, t_fine);
    uint32_t H = bme280_comp_hum(&ref_calib, ADC_H, t_fine);

    /* Temperature: datasheet says 25.08 degC = 2508 in 0.01 degC units. */
    check_exact("temp (0.01C)", T, 2508);

    /* Pressure: physical range 85000–110000 Pa.  Formula output is Pa*256. */
    check_range("press (Pa*256)", P,
                85000u * 256u, 110000u * 256u, "Pa*256");

    /* Humidity: physical range 0–100 %RH.  Formula output is %RH*1024. */
    check_range("hum (%RH*1024)", H,
                0u, 100u * 1024u, "%RH*1024");

    printf("\n%s\n", fail ? "TESTS FAILED" : "all tests passed");
    return fail;
}
