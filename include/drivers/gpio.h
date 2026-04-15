/**
 * @file  gpio.h
 * @brief GPIO driver interface for STM32F1xx
 *
 * Provides pin-level configuration and control functions that operate
 * directly on peripheral registers — no HAL dependency.
 */

#ifndef DRIVERS_GPIO_H
#define DRIVERS_GPIO_H

#include <stdint.h>
#include "stm32f1xx.h"

/**
 * @brief GPIO pin numbers (0–15)
 */
typedef enum {
    GPIO_PIN_0  = 0,
    GPIO_PIN_1  = 1,
    GPIO_PIN_2  = 2,
    GPIO_PIN_3  = 3,
    GPIO_PIN_4  = 4,
    GPIO_PIN_5  = 5,
    GPIO_PIN_6  = 6,
    GPIO_PIN_7  = 7,
    GPIO_PIN_8  = 8,
    GPIO_PIN_9  = 9,
    GPIO_PIN_10 = 10,
    GPIO_PIN_11 = 11,
    GPIO_PIN_12 = 12,
    GPIO_PIN_13 = 13,
    GPIO_PIN_14 = 14,
    GPIO_PIN_15 = 15,
} gpio_pin_t;

/**
 * @brief Configure a GPIO pin's mode
 *
 * @param port  GPIO port (GPIOA, GPIOB, GPIOC, GPIOD)
 * @param pin   Pin number (0–15)
 * @param mode  4-bit mode/config value (see GPIO_MODE_* defines)
 */
void gpio_pin_config(GPIO_TypeDef *port, gpio_pin_t pin, uint32_t mode);

/**
 * @brief Set a GPIO output pin high
 */
void gpio_pin_set(GPIO_TypeDef *port, gpio_pin_t pin);

/**
 * @brief Set a GPIO output pin low
 */
void gpio_pin_reset(GPIO_TypeDef *port, gpio_pin_t pin);

/**
 * @brief Toggle a GPIO output pin
 */
void gpio_pin_toggle(GPIO_TypeDef *port, gpio_pin_t pin);

/**
 * @brief Read the current state of a GPIO input pin
 *
 * @return 1 if high, 0 if low
 */
uint32_t gpio_pin_read(GPIO_TypeDef *port, gpio_pin_t pin);

#endif /* DRIVERS_GPIO_H */
