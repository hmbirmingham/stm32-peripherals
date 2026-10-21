#ifndef BRR_H
#define BRR_H

#include <stdint.h>

/*
 * USART BRR value for OVER8 = 0 (16x oversampling).
 * USARTDIV = fck / (16 * baud), and BRR holds USARTDIV in 12.4 fixed point,
 * so BRR = USARTDIV * 16 = fck / baud, rounded to nearest.
 */
static inline uint32_t brr_from(uint32_t fck, uint32_t baud)
{
    return (fck + baud / 2u) / baud;
}

#endif
