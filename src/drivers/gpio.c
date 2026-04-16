/**
 * @file  gpio.c
 * @brief GPIO driver implementation for STM32F1xx
 */

#include "drivers/gpio.h"

void gpio_pin_config(GPIO_TypeDef *port, gpio_pin_t pin, uint32_t mode)
{
    /*
     * STM32F1 GPIO config: each pin uses a 4-bit field.
     *   CRL covers pins 0–7, CRH covers pins 8–15.
     */
    volatile uint32_t *cr = (pin < 8) ? &port->CRL : &port->CRH;
    uint32_t shift = ((uint32_t)pin % 8U) * 4U;  /* 4 bits per pin field */

    REG_MODIFY(*cr, (0xFUL << shift), ((mode & 0xFUL) << shift));
}

void gpio_pin_set(GPIO_TypeDef *port, gpio_pin_t pin)
{
    port->BSRR = (1UL << (uint32_t)pin);
}

void gpio_pin_reset(GPIO_TypeDef *port, gpio_pin_t pin)
{
    port->BRR = (1UL << (uint32_t)pin);
}

void gpio_pin_toggle(GPIO_TypeDef *port, gpio_pin_t pin)
{
    port->ODR ^= (1UL << (uint32_t)pin);
}

uint32_t gpio_pin_read(GPIO_TypeDef *port, gpio_pin_t pin)
{
    return (port->IDR >> (uint32_t)pin) & 1UL;
}
