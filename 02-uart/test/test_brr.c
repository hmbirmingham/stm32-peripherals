#include <stdio.h>
#include <stdint.h>
#include "../Inc/brr.h"

static int fails;

static void check(uint32_t fck, uint32_t baud, uint32_t want)
{
    uint32_t got = brr_from(fck, baud);
    if (got != want) {
        printf("FAIL fck=%u baud=%u got=0x%X want=0x%X\n",
               (unsigned)fck, (unsigned)baud, (unsigned)got, (unsigned)want);
        fails++;
    }
}

int main(void)
{
    check(16000000u, 115200u, 0x8Bu);   /* HSI, 115200: mantissa 8, fraction 11 */
    check(16000000u, 9600u,   0x683u);  /* mantissa 104, fraction 3 */
    check(42000000u, 115200u, 0x16Du);  /* APB1 at 42 MHz */

    /* Regression: the old value (138<<4)|14 = 0x8AE gave ~7200 baud at 16 MHz. */
    uint32_t old_brr = (138u << 4) | 14u;
    uint32_t implied = (16000000u + old_brr / 2u) / old_brr;
    if (implied < 7100u || implied > 7300u) {
        printf("FAIL regression: old BRR implies %u baud\n", (unsigned)implied);
        fails++;
    }

    if (fails == 0) printf("all brr tests passed\n");
    return fails ? 1 : 0;
}
