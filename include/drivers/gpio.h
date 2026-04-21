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
 * @brief GPIO port identifier for clock enable and EXTI source selection
 */
typedef enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B = 1,
    GPIO_PORT_C = 2,
    GPIO_PORT_D = 3,
} gpio_port_id_t;

/**
 * @brief Edge trigger selection for EXTI interrupt configuration
 */
typedef enum {
    GPIO_EXTI_RISING  = 0x01,
    GPIO_EXTI_FALLING = 0x02,
    GPIO_EXTI_BOTH    = 0x03,
} gpio_exti_trigger_t;

/* ── Clock control ──────────────────────────────────────────────────────── */

/**
 * @brief Enable the peripheral clock for a GPIO port
 *
 * Must be called before any other operation on the port.
 *
 * @param port_id  Port identifier (GPIO_PORT_A … GPIO_PORT_D)
 */
void gpio_clock_enable(gpio_port_id_t port_id);

/* ── Pin configuration ──────────────────────────────────────────────────── */

/**
 * @brief Configure a GPIO pin's mode
 *
 * @param port  GPIO port (GPIOA, GPIOB, GPIOC, GPIOD)
 * @param pin   Pin number (0–15)
 * @param mode  4-bit mode/config value (see GPIO_MODE_* defines)
 */
void gpio_pin_config(GPIO_TypeDef *port, gpio_pin_t pin, uint32_t mode);

/**
 * @brief Configure a pin as input with internal pull-up
 *
 * Sets the pin to input-with-pull mode and drives ODR high to
 * select the pull-up resistor.
 */
void gpio_pin_config_input_pullup(GPIO_TypeDef *port, gpio_pin_t pin);

/**
 * @brief Configure a pin as input with internal pull-down
 *
 * Sets the pin to input-with-pull mode and drives ODR low to
 * select the pull-down resistor.
 */
void gpio_pin_config_input_pulldown(GPIO_TypeDef *port, gpio_pin_t pin);

/* ── Pin output control ─────────────────────────────────────────────────── */

/**
 * @brief Set a GPIO output pin high (atomic via BSRR)
 */
void gpio_pin_set(GPIO_TypeDef *port, gpio_pin_t pin);

/**
 * @brief Set a GPIO output pin low (atomic via BRR)
 */
void gpio_pin_reset(GPIO_TypeDef *port, gpio_pin_t pin);

/**
 * @brief Toggle a GPIO output pin
 */
void gpio_pin_toggle(GPIO_TypeDef *port, gpio_pin_t pin);

/**
 * @brief Write a logical value to a GPIO output pin
 *
 * @param value  Non-zero sets the pin high, zero sets it low
 */
void gpio_pin_write(GPIO_TypeDef *port, gpio_pin_t pin, uint32_t value);

/* ── Pin input ──────────────────────────────────────────────────────────── */

/**
 * @brief Read the current state of a GPIO input pin
 *
 * @return 1 if high, 0 if low
 */
uint32_t gpio_pin_read(GPIO_TypeDef *port, gpio_pin_t pin);

/* ── Port-wide operations ───────────────────────────────────────────────── */

/**
 * @brief Read all 16 pins of a GPIO port at once
 *
 * @return 16-bit value representing pin states (bit 0 = pin 0)
 */
uint16_t gpio_port_read(GPIO_TypeDef *port);

/**
 * @brief Write all 16 pins of a GPIO port at once
 *
 * @param value  16-bit value to write to ODR
 */
void gpio_port_write(GPIO_TypeDef *port, uint16_t value);

/* ── Configuration lock ─────────────────────────────────────────────────── */

/**
 * @brief Lock the configuration of selected pins
 *
 * Executes the LCKR lock sequence so that the CRL/CRH bits for the
 * specified pins cannot be changed until the next MCU reset.
 *
 * @param port      GPIO port
 * @param pin_mask  Bitmask of pins to lock (bit 0 = pin 0, etc.)
 * @return 0 on success, -1 if the lock sequence failed
 */
int gpio_pin_lock(GPIO_TypeDef *port, uint16_t pin_mask);

/* ── External interrupt configuration ───────────────────────────────────── */

/**
 * @brief Configure a GPIO pin as an EXTI interrupt source
 *
 * Routes the selected pin through AFIO to the corresponding EXTI line,
 * sets the trigger edge, and enables the interrupt in both EXTI and NVIC.
 * The AFIO clock is enabled automatically.
 *
 * @param port_id  Port identifier (selects AFIO_EXTICRx value)
 * @param pin      Pin number — also determines the EXTI line number
 * @param trigger  Edge selection (rising, falling, or both)
 */
void gpio_exti_config(gpio_port_id_t port_id, gpio_pin_t pin,
                       gpio_exti_trigger_t trigger);

/**
 * @brief Clear the EXTI pending flag for a given pin/line
 *
 * Call this at the start of the EXTI ISR to acknowledge the interrupt.
 *
 * @param pin  Pin number (same as EXTI line number)
 */
void gpio_exti_clear_pending(gpio_pin_t pin);

#endif /* DRIVERS_GPIO_H */
