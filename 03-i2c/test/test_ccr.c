#include <stdio.h>
#include <stdint.h>
#include "../Inc/ccr.h"

static int fails;

static void check(const char *label, uint32_t got, uint32_t want)
{
    if (got != want) {
        printf("FAIL %s: got=%u want=%u\n", label, (unsigned)got, (unsigned)want);
        fails++;
    }
}

int main(void)
{
    /* 16 MHz HSI: the clock this project actually runs on */
    check("cr2_freq 16MHz",  i2c_cr2_freq(16000000u),  16u);
    check("ccr_sm   16MHz",  i2c_ccr_sm(16000000u),    80u);
    check("trise_sm 16MHz",  i2c_trise_sm(16000000u),  17u);

    /* 42 MHz APB1 (168 MHz sysclk / 4, common for the F4 family) */
    check("cr2_freq 42MHz",  i2c_cr2_freq(42000000u),  42u);
    check("ccr_sm   42MHz",  i2c_ccr_sm(42000000u),   210u);
    check("trise_sm 42MHz",  i2c_trise_sm(42000000u),  43u);

    /* 8 MHz: verify no truncation on a slower clock */
    check("ccr_sm   8MHz",   i2c_ccr_sm(8000000u),     40u);
    check("trise_sm 8MHz",   i2c_trise_sm(8000000u),    9u);

    if (fails == 0) printf("all ccr tests passed\n");
    return fails ? 1 : 0;
}
