#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))

#define RCC_AHB1ENR REG(0x40023830u)
#define GPIOC_MODER REG(0x40020800u)
#define GPIOC_ODR REG(0x40020814u)
#define SYSTICK_CTRL REG(0xE000E010u)
#define SYSTICK_LOAD REG(0xE000E014u)
#define SYSTICK_VAL REG(0xE000E018u)

static void delay_ms(uint32_t milliseconds)
{
    while (milliseconds-- != 0u) {
        SYSTICK_LOAD = 16000u - 1u;
        SYSTICK_VAL = 0;
        SYSTICK_CTRL = 5u;
        while ((SYSTICK_CTRL & (1u << 16)) == 0u) {
        }
    }

    SYSTICK_CTRL = 0;
}

int main(void)
{
    RCC_AHB1ENR |= 1u << 2;
    GPIOC_MODER = (GPIOC_MODER & ~(3u << 26)) | (1u << 26);
    GPIOC_ODR |= 1u << 13;

    for (;;) {
        GPIOC_ODR &= ~(1u << 13);
        delay_ms(500u);
        GPIOC_ODR |= 1u << 13;
        delay_ms(500u);
    }
}
