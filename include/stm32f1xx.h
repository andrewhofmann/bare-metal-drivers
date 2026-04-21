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

/* GPIO configuration modes (for CRL/CRH 4-bit fields)
 *
 * Each 4-bit field encodes CNF[1:0]:MODE[1:0]:
 *   MODE = 00 (input), 01 (10 MHz), 10 (2 MHz), 11 (50 MHz)
 *   CNF  = depends on input vs output, see reference manual §9.2
 */
/* Input modes (MODE = 00) */
#define GPIO_MODE_INPUT_ANALOG      0x0UL   /* CNF=00: Analog input           */
#define GPIO_MODE_INPUT_FLOATING    0x4UL   /* CNF=01: Floating input         */
#define GPIO_MODE_INPUT_PULL        0x8UL   /* CNF=10: Pull-up / pull-down    */

/* General-purpose output — push-pull */
#define GPIO_MODE_OUTPUT_PP_2MHZ    0x2UL   /* CNF=00, MODE=10                */
#define GPIO_MODE_OUTPUT_PP_10MHZ   0x1UL   /* CNF=00, MODE=01                */
#define GPIO_MODE_OUTPUT_PP_50MHZ   0x3UL   /* CNF=00, MODE=11                */

/* General-purpose output — open-drain */
#define GPIO_MODE_OUTPUT_OD_2MHZ    0x6UL   /* CNF=01, MODE=10                */
#define GPIO_MODE_OUTPUT_OD_10MHZ   0x5UL   /* CNF=01, MODE=01                */
#define GPIO_MODE_OUTPUT_OD_50MHZ   0x7UL   /* CNF=01, MODE=11                */

/* Alternate-function output — push-pull */
#define GPIO_MODE_AF_PP_2MHZ        0xAUL   /* CNF=10, MODE=10                */
#define GPIO_MODE_AF_PP_10MHZ       0x9UL   /* CNF=10, MODE=01                */
#define GPIO_MODE_AF_PP_50MHZ       0xBUL   /* CNF=10, MODE=11                */

/* Alternate-function output — open-drain */
#define GPIO_MODE_AF_OD_2MHZ        0xEUL   /* CNF=11, MODE=10                */
#define GPIO_MODE_AF_OD_10MHZ       0xDUL   /* CNF=11, MODE=01                */
#define GPIO_MODE_AF_OD_50MHZ       0xFUL   /* CNF=11, MODE=11                */

/* GPIO lock key (bit 16 of LCKR) */
#define GPIO_LCKR_LCKK             (1UL << 16)

/* ---------------------------------------------------------------------------
 * AFIO — Alternate Function I/O
 * ------------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t EVCR;         /* 0x00: Event control               */
    volatile uint32_t MAPR;         /* 0x04: Remap and debug config      */
    volatile uint32_t EXTICR[4];    /* 0x08–0x14: EXTI line source sel.  */
    volatile uint32_t RESERVED;     /* 0x18                              */
    volatile uint32_t MAPR2;        /* 0x1C: Remap and debug config 2    */
} AFIO_TypeDef;

#define AFIO    ((AFIO_TypeDef *)AFIO_BASE)

/* AFIO_EXTICR port selection values (4 bits per EXTI line) */
#define AFIO_EXTICR_PA              0x0UL
#define AFIO_EXTICR_PB              0x1UL
#define AFIO_EXTICR_PC              0x2UL
#define AFIO_EXTICR_PD              0x3UL

/* ---------------------------------------------------------------------------
 * EXTI — External Interrupt / Event Controller
 * ------------------------------------------------------------------------ */
#define EXTI_BASE       0x40010400UL

typedef struct {
    volatile uint32_t IMR;          /* 0x00: Interrupt mask               */
    volatile uint32_t EMR;          /* 0x04: Event mask                   */
    volatile uint32_t RTSR;         /* 0x08: Rising trigger selection     */
    volatile uint32_t FTSR;         /* 0x0C: Falling trigger selection    */
    volatile uint32_t SWIER;        /* 0x10: Software interrupt event     */
    volatile uint32_t PR;           /* 0x14: Pending                      */
} EXTI_TypeDef;

#define EXTI    ((EXTI_TypeDef *)EXTI_BASE)

/* ---------------------------------------------------------------------------
 * NVIC — Nested Vectored Interrupt Controller (partial)
 * ------------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t ISER[8];      /* 0x000: Interrupt set-enable        */
    uint32_t RESERVED0[24];
    volatile uint32_t ICER[8];      /* 0x080: Interrupt clear-enable      */
    uint32_t RESERVED1[24];
    volatile uint32_t ISPR[8];      /* 0x100: Interrupt set-pending       */
    uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8];      /* 0x180: Interrupt clear-pending     */
    uint32_t RESERVED3[24];
    volatile uint32_t IABR[8];      /* 0x200: Interrupt active bit        */
    uint32_t RESERVED4[56];
    volatile uint32_t IP[240];      /* 0x300: Interrupt priority (8-bit)  */
} NVIC_TypeDef;

#define NVIC    ((NVIC_TypeDef *)NVIC_BASE)

/* EXTI IRQ numbers in the NVIC (STM32F103) */
#define EXTI0_IRQn      6
#define EXTI1_IRQn      7
#define EXTI2_IRQn      8
#define EXTI3_IRQn      9
#define EXTI4_IRQn      10
#define EXTI9_5_IRQn    23
#define EXTI15_10_IRQn  40

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
