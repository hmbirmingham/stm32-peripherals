#include <stdint.h>

typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
} USART_TypeDef;

#define USART2 ((USART_TypeDef *)0x40004400)

// BRR = (mantissa << 4) | fraction
// mantissa = 22 (0x16), fraction = 12 (0xC) for 115200 baud @ 42MHz APB1
void uart_init(void)
{
    USART2->BRR = 0x16C;
}
