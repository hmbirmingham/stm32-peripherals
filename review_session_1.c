#include <stdint.h>

typedef struct{
	volatile uint32_t AHB1ENR;
} RCC_TypeDef;

typedef struct {
	volatile uint32_t MODER;
	volatile uint32_t ODR;
} GPIO_TypeDef;

#define RCC ((RCC_TypeDef *)0x40023800)
#define GPIOA ((GPIO_TypeDef *)0x40020000)

#define RCC_AHB1ENR_GPIOAEN (1U << 0)
#define GPIOA_PIN5_MODER_POS (5U * 2U)
#define GPIOA_PIN5_MODER_MASK (0x3U << GPIOA_PIN5_MODER_POS)
#define GPIOA_PIN5_MODE_OUTPUT (0x1U << GPIOA_PIN5_MODER_POS)
#define GPIOA_PIN5 (1U << 5)

void init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

	GPIOA->MODER &= ~GPIOA_PIN5_MODER_MASK;
	GPIOA->MODER |= GPIOA_PIN5_MODE_OUTPUT;

	GPIOA->ODR |= GPIOA_PIN5;
}
