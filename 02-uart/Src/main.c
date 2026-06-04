#include <stdint.h>

// RCC
#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830)
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840)

// GPIOA
#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000)
#define GPIOA_AFRL   (*(volatile uint32_t *)0x40020020)

// USART2
#define USART2_SR    (*(volatile uint32_t *)0x40004400)
#define USART2_DR    (*(volatile uint32_t *)0x40004404)
#define USART2_BRR   (*(volatile uint32_t *)0x40004408)
#define USART2_CR1   (*(volatile uint32_t *)0x4000440C)

void uart_init(void) {
    // Enable GPIOA and USART2 clocks
    RCC_AHB1ENR |= (1 << 0);   // GPIOA
    RCC_APB1ENR |= (1 << 17);  // USART2

    // Set PA2 to alternate function mode (USART2 TX)
    GPIOA_MODER &= ~(0x3 << 4);
    GPIOA_MODER |=  (0x2 << 4);

    // Set PA2 alternate function to AF7 (USART2)
    GPIOA_AFRL &= ~(0xF << 8);
    GPIOA_AFRL |=  (0x7 << 8);

    // Set baud rate to 115200 (assuming 16MHz clock)
    // BRR = 16000000 / 115200 = 138.88 → mantissa=138, fraction=14
    USART2_BRR = (138 << 4) | 14;

    // Enable USART2, TX
    USART2_CR1 |= (1 << 13) | (1 << 3);
}

void uart_send_char(char c) {
    // Wait until TX buffer empty
    while (!(USART2_SR & (1 << 7)));
    USART2_DR = c;
}

void uart_send_string(const char *str) {
    while (*str) {
        uart_send_char(*str++);
    }
}

int main(void) {
    uart_init();

    while (1) {
        uart_send_string("Hello from STM32\r\n");
        for (volatile int i = 0; i < 1000000; i++);
    }
}
