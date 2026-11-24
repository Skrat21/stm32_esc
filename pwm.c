/*
 * STM32F401RCT6 (Black Pill) - bare metal ESC signal generator
 * Output: PA0 (TIM2_CH1), 50 Hz, pulse width 1600 us
 * Clock : default HSI 16 MHz (no clock setup needed)
 *
 * Startup: holds 1500 us (neutral) for ~3 s so the ESC can arm,
 *          then switches to 1600 us permanently.
 */
#include <stdint.h>

#define REG(addr)   (*(volatile uint32_t *)(addr))

/* RCC */
#define RCC_CR      REG(0x40023800)
#define RCC_CFGR    REG(0x40023808)
#define RCC_AHB1ENR REG(0x40023830)
#define RCC_APB1ENR REG(0x40023840)

/* GPIOA */
#define GPIOA_MODER REG(0x40020000)
#define GPIOA_AFRL  REG(0x40020020)

/* TIM2 */
#define TIM2_CR1    REG(0x40000000)
#define TIM2_SR     REG(0x40000010)
#define TIM2_EGR    REG(0x40000014)
#define TIM2_CCMR1  REG(0x40000018)
#define TIM2_CCER   REG(0x40000020)
#define TIM2_PSC    REG(0x40000028)
#define TIM2_ARR    REG(0x4000002C)
#define TIM2_CCR1   REG(0x40000034)

#define PULSE_NEUTRAL_US  1500
#define PULSE_RUN_US      1550
#define ARM_TIME_FRAMES   500      /* 500 * 20 ms = 10 s */

/* GPIOC (onboard LED on PC13, active low) */
#define GPIOC_MODER REG(0x40020800)
#define GPIOC_ODR   REG(0x40020814)

int main(void)
{
    /* Try external 25 MHz crystal (HSE) for accurate timing.
       Falls back to internal 16 MHz HSI if it doesn't start. */
    uint32_t psc = 16 - 1;                       /* HSI: 16 MHz / 16 = 1 MHz */
    RCC_CR |= (1u << 16);                        /* HSEON */
    uint32_t timeout = 200000;
    while (!(RCC_CR & (1u << 17)) && --timeout) { }   /* wait HSERDY */
    if (RCC_CR & (1u << 17)) {
        RCC_CFGR = (RCC_CFGR & ~3u) | 1u;        /* SYSCLK = HSE */
        while (((RCC_CFGR >> 2) & 3u) != 1u) { } /* wait switch */
        psc = 25 - 1;                            /* HSE: 25 MHz / 25 = 1 MHz */
    }

    /* Clocks: GPIOA, TIM2 */
    RCC_AHB1ENR |= (1u << 0) | (1u << 2);
    RCC_APB1ENR |= (1u << 0);

    /* PC13 LED output: ON (low) while arming, OFF when running */
    GPIOC_MODER = (GPIOC_MODER & ~(3u << 26)) | (1u << 26);
    GPIOC_ODR &= ~(1u << 13);

    /* PA0 -> alternate function AF1 (TIM2_CH1) */
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 0)) | (2u << 0);
    GPIOA_AFRL  = (GPIOA_AFRL  & ~(0xFu << 0)) | (1u << 0);

    /* Timer: 16 MHz / 16 = 1 MHz -> 1 tick = 1 us, period 20 ms = 50 Hz */
    TIM2_PSC  = psc;
    TIM2_ARR  = 20000 - 1;
    TIM2_CCR1 = PULSE_NEUTRAL_US;

    /* CH1: PWM mode 1, preload enabled */
    TIM2_CCMR1 = (6u << 4) | (1u << 3);
    TIM2_CCER  = (1u << 0);              /* enable CH1 output */
    TIM2_CR1   = (1u << 7);              /* ARPE */

    TIM2_EGR  = 1u;                      /* load registers */
    TIM2_SR   = 0;
    TIM2_CR1 |= 1u;                      /* start */

    /* Arm the ESC: 1500 us for ~3 s */
    for (int i = 0; i < ARM_TIME_FRAMES; i++) {
        while (!(TIM2_SR & 1u)) { }
        TIM2_SR = 0;
    }

    /* Run: 1600 us */
    TIM2_CCR1 = PULSE_RUN_US;
    GPIOC_ODR |= (1u << 13);             /* LED off = running at 1600 */

    while (1) { }
}

/* ---------------- Minimal startup (no CMSIS needed) ---------------- */
extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void)
{
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;
    main();
    while (1) { }
}

void Default_Handler(void) { while (1) { } }

__attribute__((section(".isr_vector"), used))
void (* const vector_table[])(void) = {
    (void (*)(void))&_estack,
    Reset_Handler,
    Default_Handler,   /* NMI */
    Default_Handler,   /* HardFault */
};