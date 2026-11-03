#include <stdint.h>
#include "../Inc/brr.h"

/* RCC */
#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830)
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840)

#define RCC_AHB1ENR_GPIOAEN   (1u << 0)
#define RCC_APB1ENR_USART2EN  (1u << 17)

/* GPIOA. PA2 is USART2_TX, PA3 is USART2_RX, both on AF7. */
#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000)
#define GPIOA_AFRL   (*(volatile uint32_t *)0x40020020)

#define MODER_MASK      0x3u
#define MODER_AF        0x2u
#define AFRL_MASK       0xFu
#define AF7             0x7u
#define PA2_MODER_POS   (2u * 2u)
#define PA3_MODER_POS   (3u * 2u)
#define PA2_AFRL_POS    (2u * 4u)
#define PA3_AFRL_POS    (3u * 4u)

/* USART2 */
#define USART2_SR   (*(volatile uint32_t *)0x40004400)
#define USART2_DR   (*(volatile uint32_t *)0x40004404)
#define USART2_BRR  (*(volatile uint32_t *)0x40004408)
#define USART2_CR1  (*(volatile uint32_t *)0x4000440C)

#define USART_SR_PE     (1u << 0)
#define USART_SR_FE     (1u << 1)
#define USART_SR_NE     (1u << 2)
#define USART_SR_ORE    (1u << 3)
#define USART_SR_RXNE   (1u << 5)
#define USART_SR_TXE    (1u << 7)

#define USART_CR1_RE      (1u << 2)
#define USART_CR1_TE      (1u << 3)
#define USART_CR1_RXNEIE  (1u << 5)
#define USART_CR1_TXEIE   (1u << 7)
#define USART_CR1_UE      (1u << 13)

/* USART2 is IRQ 38, counted from the vector table in Startup. ISER1 covers
 * IRQ 32 to 63, so the bit is 38 minus 32. */
#define NVIC_ISER1   (*(volatile uint32_t *)0xE000E104)
#define USART2_IRQn  38u

/* Power of two so the wrap is a mask instead of a modulo. */
#define TX_BUF_SIZE  64u
#define TX_BUF_MASK  (TX_BUF_SIZE - 1u)

static volatile uint8_t  tx_buf[TX_BUF_SIZE];
static volatile uint32_t tx_head;  /* only main writes this */
static volatile uint32_t tx_tail;  /* only the ISR writes this */

/* Counters I can read with the debugger on the bench. */
volatile uint32_t uart_err_framing;
volatile uint32_t uart_err_overrun;
volatile uint32_t uart_err_noise;
volatile uint32_t uart_err_parity;
volatile uint32_t uart_tx_dropped;
volatile uint8_t  uart_last_rx;

void uart_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA_MODER &= ~(MODER_MASK << PA2_MODER_POS);
    GPIOA_MODER |=  (MODER_AF   << PA2_MODER_POS);
    GPIOA_MODER &= ~(MODER_MASK << PA3_MODER_POS);
    GPIOA_MODER |=  (MODER_AF   << PA3_MODER_POS);

    GPIOA_AFRL &= ~(AFRL_MASK << PA2_AFRL_POS);
    GPIOA_AFRL |=  (AF7       << PA2_AFRL_POS);
    GPIOA_AFRL &= ~(AFRL_MASK << PA3_AFRL_POS);
    GPIOA_AFRL |=  (AF7       << PA3_AFRL_POS);

    /* 115200 on the 16 MHz HSI with OVER8 = 0: BRR = fck / baud = 139 = 0x8B */
    USART2_BRR = brr_from(16000000u, 115200u);

    NVIC_ISER1 = (1u << (USART2_IRQn - 32u));

    USART2_CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
}

/* Queues up to len bytes and returns how many it took. Never blocks. If the
 * buffer is full the surplus is dropped and counted, because a driver that
 * spins here would defeat the point of moving transmission into the ISR. */
uint32_t uart_write(const char *data, uint32_t len)
{
    uint32_t taken = 0;

    while (taken < len) {
        uint32_t head = tx_head;
        uint32_t next = (head + 1u) & TX_BUF_MASK;

        if (next == tx_tail) {
            break;
        }
        tx_buf[head] = (uint8_t)data[taken];
        tx_head = next;
        taken++;
    }

    uart_tx_dropped += (len - taken);

    if (tx_head != tx_tail) {
        USART2_CR1 |= USART_CR1_TXEIE;
    }
    return taken;
}

uint32_t uart_puts(const char *s)
{
    uint32_t n = 0;

    while (s[n] != '\0') {
        n++;
    }
    return uart_write(s, n);
}

void USART2_IRQHandler(void)
{
    uint32_t sr = USART2_SR;

    /* Receive errors come first. On the F4 these flags clear on a read of SR
     * followed by a read of DR, so the DR read below is what clears them.
     * Writing to SR does not work, which is the part that trips people up. */
    if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        if (sr & USART_SR_FE)  { uart_err_framing++; }
        if (sr & USART_SR_ORE) { uart_err_overrun++; }
        if (sr & USART_SR_NE)  { uart_err_noise++;   }
        if (sr & USART_SR_PE)  { uart_err_parity++;  }
        (void)USART2_DR;
    } else if (sr & USART_SR_RXNE) {
        uart_last_rx = (uint8_t)USART2_DR;
    }

    if ((sr & USART_SR_TXE) && (USART2_CR1 & USART_CR1_TXEIE)) {
        if (tx_tail == tx_head) {
            /* Buffer drained. Stop asking for TXE or the ISR will spin. */
            USART2_CR1 &= ~USART_CR1_TXEIE;
        } else {
            USART2_DR = tx_buf[tx_tail];
            tx_tail = (tx_tail + 1u) & TX_BUF_MASK;
        }
    }
}

int main(void)
{
    uart_init();

    for (;;) {
        /* Only queue the next line once the ISR has drained the last one, so
         * the 18 byte message never gets chopped by a full buffer. The queue
         * call itself does not block either way. */
        if (tx_head == tx_tail) {
            uart_puts("Hello from STM32\r\n");
        }

        for (volatile uint32_t i = 0; i < 400000u; i++) {
        }
    }
}
