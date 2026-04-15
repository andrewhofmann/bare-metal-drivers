/**
 * @file  stm32f1xx.h
 * @brief Minimal register definitions for STM32F1xx peripherals
 *
 * Defines base addresses and register structures for the peripherals
 * used by the bare-metal drivers. Add registers as new drivers are
 * implemented — keep this file lean.
 */

#ifndef STM32F1XX_H
#define STM32F1XX_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Peripheral base addresses
 * ------------------------------------------------------------------------ */

/* AHB */
#define RCC_BASE        0x40021000UL

/* APB2 */
#define GPIOA_BASE      0x40010800UL
#define GPIOB_BASE      0x40010C00UL
#define GPIOC_BASE      0x40011000UL
#define GPIOD_BASE      0x40011400UL
#define AFIO_BASE       0x40010000UL
#define USART1_BASE     0x40013800UL
#define SPI1_BASE       0x40013000UL
#define TIM1_BASE       0x40012C00UL

/* APB1 */
#define USART2_BASE     0x40004400UL
#define USART3_BASE     0x40004800UL
#define SPI2_BASE       0x40003800UL
#define I2C1_BASE       0x40005400UL
#define I2C2_BASE       0x40005800UL
#define TIM2_BASE       0x40000000UL
#define TIM3_BASE       0x40000400UL
#define TIM4_BASE       0x40000800UL

/* Cortex-M3 system */
#define SYSTICK_BASE    0xE000E010UL
#define NVIC_BASE       0xE000E100UL
#define SCB_BASE        0xE000ED00UL

/* ---------------------------------------------------------------------------
 * RCC — Reset and Clock Control
 * ------------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t CR;           /* 0x00: Clock control              */
    volatile uint32_t CFGR;         /* 0x04: Clock configuration        */
    volatile uint32_t CIR;          /* 0x08: Clock interrupt            */
    volatile uint32_t APB2RSTR;     /* 0x0C: APB2 peripheral reset      */
    volatile uint32_t APB1RSTR;     /* 0x10: APB1 peripheral reset      */
    volatile uint32_t AHBENR;       /* 0x14: AHB peripheral clock en.   */
    volatile uint32_t APB2ENR;      /* 0x18: APB2 peripheral clock en.  */
    volatile uint32_t APB1ENR;      /* 0x1C: APB1 peripheral clock en.  */
    volatile uint32_t BDCR;         /* 0x20: Backup domain control      */
    volatile uint32_t CSR;          /* 0x24: Control / status           */
} RCC_TypeDef;

#define RCC     ((RCC_TypeDef *)RCC_BASE)

/* RCC_APB2ENR bit positions */
#define RCC_APB2ENR_IOPAEN  (1UL << 2)
#define RCC_APB2ENR_IOPBEN  (1UL << 3)
#define RCC_APB2ENR_IOPCEN  (1UL << 4)
#define RCC_APB2ENR_IOPDEN  (1UL << 5)
#define RCC_APB2ENR_AFIOEN  (1UL << 0)
#define RCC_APB2ENR_USART1EN (1UL << 14)
#define RCC_APB2ENR_SPI1EN  (1UL << 12)
#define RCC_APB2ENR_TIM1EN  (1UL << 11)

/* RCC_APB1ENR bit positions */
#define RCC_APB1ENR_USART2EN (1UL << 17)
#define RCC_APB1ENR_USART3EN (1UL << 18)
#define RCC_APB1ENR_SPI2EN  (1UL << 14)
#define RCC_APB1ENR_I2C1EN  (1UL << 21)
#define RCC_APB1ENR_I2C2EN  (1UL << 22)
#define RCC_APB1ENR_TIM2EN  (1UL << 0)
#define RCC_APB1ENR_TIM3EN  (1UL << 1)
#define RCC_APB1ENR_TIM4EN  (1UL << 2)

/* ---------------------------------------------------------------------------
 * GPIO — General-Purpose I/O
 * ------------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t CRL;          /* 0x00: Port configuration low     */
    volatile uint32_t CRH;          /* 0x04: Port configuration high    */
    volatile uint32_t IDR;          /* 0x08: Input data                 */
    volatile uint32_t ODR;          /* 0x0C: Output data                */
    volatile uint32_t BSRR;         /* 0x10: Bit set/reset              */
    volatile uint32_t BRR;          /* 0x14: Bit reset                  */
    volatile uint32_t LCKR;         /* 0x18: Configuration lock         */
} GPIO_TypeDef;

#define GPIOA   ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB   ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC   ((GPIO_TypeDef *)GPIOC_BASE)
#define GPIOD   ((GPIO_TypeDef *)GPIOD_BASE)

/* GPIO configuration modes (for CRL/CRH 4-bit fields) */
#define GPIO_MODE_INPUT_ANALOG      0x0UL
#define GPIO_MODE_INPUT_FLOATING    0x4UL
#define GPIO_MODE_INPUT_PULL        0x8UL
#define GPIO_MODE_OUTPUT_PP_2MHZ    0x2UL
#define GPIO_MODE_OUTPUT_PP_10MHZ   0x1UL
#define GPIO_MODE_OUTPUT_PP_50MHZ   0x3UL
#define GPIO_MODE_OUTPUT_OD_2MHZ    0x6UL
#define GPIO_MODE_AF_PP_50MHZ       0xBUL
#define GPIO_MODE_AF_OD_50MHZ       0xFUL

/* ---------------------------------------------------------------------------
 * SysTick — System Timer
 * ------------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t CTRL;         /* 0x00: Control and status         */
    volatile uint32_t LOAD;         /* 0x04: Reload value               */
    volatile uint32_t VAL;          /* 0x08: Current value              */
    volatile uint32_t CALIB;        /* 0x0C: Calibration                */
} SysTick_TypeDef;

#define SYSTICK ((SysTick_TypeDef *)SYSTICK_BASE)

#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_TICKINT    (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1UL << 16)

/* ---------------------------------------------------------------------------
 * Utility macros
 * ------------------------------------------------------------------------ */

/** Set bits in a register */
#define REG_SET(reg, mask)      ((reg) |= (mask))

/** Clear bits in a register */
#define REG_CLR(reg, mask)      ((reg) &= ~(mask))

/** Modify specific bits: clear mask, then set value */
#define REG_MODIFY(reg, mask, val) \
    do { (reg) = ((reg) & ~(mask)) | (val); } while (0)

#endif /* STM32F1XX_H */
