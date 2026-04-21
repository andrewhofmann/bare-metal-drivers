/**
 * @file  gpio.c
 * @brief GPIO driver implementation for STM32F1xx
 *
 * Covers pin configuration, output control, port-wide access,
 * configuration locking, and EXTI interrupt setup.
 */

#include "drivers/gpio.h"

/* ── Clock control ──────────────────────────────────────────────────────── */

void gpio_clock_enable(gpio_port_id_t port_id)
{
    /* IOPxEN bits are at positions 2 (IOPA) through 5 (IOPD) in APB2ENR */
    REG_SET(RCC->APB2ENR, (1UL << ((uint32_t)port_id + 2U)));
}

/* ── Pin configuration ──────────────────────────────────────────────────── */

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

void gpio_pin_config_input_pullup(GPIO_TypeDef *port, gpio_pin_t pin)
{
    gpio_pin_config(port, pin, GPIO_MODE_INPUT_PULL);
    /* Pull-up is selected by setting the corresponding ODR bit high */
    port->ODR |= (1UL << (uint32_t)pin);
}

void gpio_pin_config_input_pulldown(GPIO_TypeDef *port, gpio_pin_t pin)
{
    gpio_pin_config(port, pin, GPIO_MODE_INPUT_PULL);
    /* Pull-down is selected by clearing the corresponding ODR bit */
    port->ODR &= ~(1UL << (uint32_t)pin);
}

/* ── Pin output control ─────────────────────────────────────────────────── */

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

void gpio_pin_write(GPIO_TypeDef *port, gpio_pin_t pin, uint32_t value)
{
    if (value) {
        port->BSRR = (1UL << (uint32_t)pin);
    } else {
        port->BRR = (1UL << (uint32_t)pin);
    }
}

/* ── Pin input ──────────────────────────────────────────────────────────── */

uint32_t gpio_pin_read(GPIO_TypeDef *port, gpio_pin_t pin)
{
    return (port->IDR >> (uint32_t)pin) & 1UL;
}

/* ── Port-wide operations ───────────────────────────────────────────────── */

uint16_t gpio_port_read(GPIO_TypeDef *port)
{
    return (uint16_t)(port->IDR & 0xFFFFUL);
}

void gpio_port_write(GPIO_TypeDef *port, uint16_t value)
{
    port->ODR = (uint32_t)value;
}

/* ── Configuration lock ─────────────────────────────────────────────────── */

int gpio_pin_lock(GPIO_TypeDef *port, uint16_t pin_mask)
{
    /*
     * The lock sequence (reference manual §9.2.6):
     *   1) Write LCKK=1 | pin_mask
     *   2) Write LCKK=0 | pin_mask
     *   3) Write LCKK=1 | pin_mask
     *   4) Read LCKR
     *   5) Read LCKR — LCKK should be 1 if the sequence succeeded
     */
    uint32_t pins = (uint32_t)pin_mask;

    port->LCKR = GPIO_LCKR_LCKK | pins;
    port->LCKR = pins;
    port->LCKR = GPIO_LCKR_LCKK | pins;

    /* First read (required by the lock sequence) */
    (void)port->LCKR;

    /* Second read — LCKK must read back as 1 */
    if (port->LCKR & GPIO_LCKR_LCKK) {
        return 0;   /* Lock confirmed */
    }
    return -1;      /* Lock sequence failed */
}

/* ── External interrupt configuration ───────────────────────────────────── */

/**
 * @brief Return the NVIC IRQ number for a given EXTI line (pin number)
 */
static uint32_t exti_irqn_for_pin(gpio_pin_t pin)
{
    switch ((uint32_t)pin) {
    case 0:  return EXTI0_IRQn;
    case 1:  return EXTI1_IRQn;
    case 2:  return EXTI2_IRQn;
    case 3:  return EXTI3_IRQn;
    case 4:  return EXTI4_IRQn;
    default:
        return ((uint32_t)pin <= 9) ? EXTI9_5_IRQn : EXTI15_10_IRQn;
    }
}

void gpio_exti_config(gpio_port_id_t port_id, gpio_pin_t pin,
                       gpio_exti_trigger_t trigger)
{
    uint32_t line = (uint32_t)pin;

    /* Enable AFIO clock (needed for EXTI source selection) */
    REG_SET(RCC->APB2ENR, RCC_APB2ENR_AFIOEN);

    /*
     * AFIO_EXTICRx: 4 EXTI lines per register, 4 bits each.
     *   EXTICR[0] → lines 0–3, EXTICR[1] → lines 4–7, etc.
     */
    uint32_t reg_idx = line / 4U;
    uint32_t shift   = (line % 4U) * 4U;
    REG_MODIFY(AFIO->EXTICR[reg_idx],
               (0xFUL << shift),
               ((uint32_t)port_id << shift));

    /* Configure trigger edge(s) */
    if (trigger & GPIO_EXTI_RISING) {
        REG_SET(EXTI->RTSR, (1UL << line));
    } else {
        REG_CLR(EXTI->RTSR, (1UL << line));
    }

    if (trigger & GPIO_EXTI_FALLING) {
        REG_SET(EXTI->FTSR, (1UL << line));
    } else {
        REG_CLR(EXTI->FTSR, (1UL << line));
    }

    /* Unmask the EXTI line */
    REG_SET(EXTI->IMR, (1UL << line));

    /* Enable the corresponding IRQ in the NVIC */
    uint32_t irqn = exti_irqn_for_pin(pin);
    NVIC->ISER[irqn / 32U] = (1UL << (irqn % 32U));
}

void gpio_exti_clear_pending(gpio_pin_t pin)
{
    /* Write 1 to clear the pending bit (rc_w1) */
    EXTI->PR = (1UL << (uint32_t)pin);
}
