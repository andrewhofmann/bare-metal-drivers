/**
 * @file  main.c
 * @brief Bare-metal entry point — LED blink on PC13 (Blue Pill)
 *
 * Minimal application that blinks the on-board LED to verify the
 * build system, startup code, and GPIO driver are working.
 */

#include "stm32f1xx.h"
#include "drivers/gpio.h"

/** Simple busy-wait delay (approximate, not cycle-accurate) */
static void delay(volatile uint32_t count)
{
    while (count--) {
        __asm__ volatile("nop");
    }
}

int main(void)
{
    /* Enable GPIOC peripheral clock */
    REG_SET(RCC->APB2ENR, RCC_APB2ENR_IOPCEN);

    /* Configure PC13 as push-pull output, 2 MHz */
    gpio_pin_config(GPIOC, GPIO_PIN_13, GPIO_MODE_OUTPUT_PP_2MHZ);

    while (1) {
        gpio_pin_toggle(GPIOC, GPIO_PIN_13);
        delay(200000);
    }

    return 0;
}
