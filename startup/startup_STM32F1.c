/**
 * @file  startup_STM32F1.c
 * @brief Startup code for STM32F1xx Cortex-M3 devices
 *
 * Provides:
 *   - Interrupt vector table (.isr_vector section)
 *   - Reset_Handler: copies .data, zeroes .bss, calls main()
 *   - Default_Handler: infinite loop for unhandled interrupts
 *
 * All ISR slots are weak aliases to Default_Handler so that drivers
 * can override individual handlers without modifying this file.
 */

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Symbols defined by the linker script
 * ------------------------------------------------------------------------ */
extern uint32_t _estack;    /* Initial stack pointer (top of SRAM)        */
extern uint32_t _sidata;    /* Start of .data initializers in flash       */
extern uint32_t _sdata;     /* Start of .data section in SRAM             */
extern uint32_t _edata;     /* End of .data section in SRAM               */
extern uint32_t _sbss;      /* Start of .bss section in SRAM              */
extern uint32_t _ebss;      /* End of .bss section in SRAM                */

/* ---------------------------------------------------------------------------
 * Prototypes
 * ------------------------------------------------------------------------ */
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* ---------------------------------------------------------------------------
 * Cortex-M3 system exception handlers (weak — override as needed)
 * ------------------------------------------------------------------------ */
void NMI_Handler(void)             __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)      __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)             __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)         __attribute__((weak, alias("Default_Handler")));

/* ---------------------------------------------------------------------------
 * STM32F1xx peripheral interrupt handlers (weak — override as needed)
 * ------------------------------------------------------------------------ */
void WWDG_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void PVD_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void TAMPER_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void RTC_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void FLASH_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void RCC_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void EXTI0_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void EXTI1_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void EXTI2_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void EXTI3_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void EXTI4_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel3_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel4_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel5_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel6_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel7_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void ADC1_2_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void USB_HP_CAN1_TX_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void USB_LP_CAN1_RX0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void CAN1_RX1_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void CAN1_SCE_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void EXTI9_5_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void TIM1_BRK_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void TIM1_UP_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void TIM1_TRG_COM_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIM1_CC_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void TIM2_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void TIM3_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void TIM4_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void I2C1_EV_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void I2C1_ER_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void I2C2_EV_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void I2C2_ER_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void SPI1_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void SPI2_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void USART2_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void USART3_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void EXTI15_10_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void RTC_Alarm_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void USBWakeUp_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));

/* ---------------------------------------------------------------------------
 * Interrupt vector table — placed at the start of flash by the linker
 * ------------------------------------------------------------------------ */
__attribute__((section(".isr_vector"), used))
void (*const vector_table[])(void) = {
    /* Initial stack pointer */
    (void (*)(void))(&_estack),

    /* Cortex-M3 system exceptions */
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,              /* Reserved */
    SVC_Handler,
    DebugMon_Handler,
    0,                        /* Reserved */
    PendSV_Handler,
    SysTick_Handler,

    /* STM32F1xx peripheral interrupts (IRQ 0–42) */
    WWDG_IRQHandler,          /*  0: Window Watchdog              */
    PVD_IRQHandler,           /*  1: PVD through EXTI             */
    TAMPER_IRQHandler,        /*  2: Tamper                       */
    RTC_IRQHandler,           /*  3: RTC global                   */
    FLASH_IRQHandler,         /*  4: Flash global                 */
    RCC_IRQHandler,           /*  5: RCC global                   */
    EXTI0_IRQHandler,         /*  6: EXTI Line 0                  */
    EXTI1_IRQHandler,         /*  7: EXTI Line 1                  */
    EXTI2_IRQHandler,         /*  8: EXTI Line 2                  */
    EXTI3_IRQHandler,         /*  9: EXTI Line 3                  */
    EXTI4_IRQHandler,         /* 10: EXTI Line 4                  */
    DMA1_Channel1_IRQHandler, /* 11: DMA1 Channel 1               */
    DMA1_Channel2_IRQHandler, /* 12: DMA1 Channel 2               */
    DMA1_Channel3_IRQHandler, /* 13: DMA1 Channel 3               */
    DMA1_Channel4_IRQHandler, /* 14: DMA1 Channel 4               */
    DMA1_Channel5_IRQHandler, /* 15: DMA1 Channel 5               */
    DMA1_Channel6_IRQHandler, /* 16: DMA1 Channel 6               */
    DMA1_Channel7_IRQHandler, /* 17: DMA1 Channel 7               */
    ADC1_2_IRQHandler,        /* 18: ADC1 and ADC2                */
    USB_HP_CAN1_TX_IRQHandler,  /* 19: USB High Priority / CAN TX */
    USB_LP_CAN1_RX0_IRQHandler, /* 20: USB Low Priority / CAN RX0 */
    CAN1_RX1_IRQHandler,     /* 21: CAN RX1                      */
    CAN1_SCE_IRQHandler,     /* 22: CAN SCE                      */
    EXTI9_5_IRQHandler,      /* 23: EXTI Lines 5–9                */
    TIM1_BRK_IRQHandler,     /* 24: TIM1 Break                   */
    TIM1_UP_IRQHandler,      /* 25: TIM1 Update                  */
    TIM1_TRG_COM_IRQHandler, /* 26: TIM1 Trigger / Commutation   */
    TIM1_CC_IRQHandler,      /* 27: TIM1 Capture Compare         */
    TIM2_IRQHandler,         /* 28: TIM2                          */
    TIM3_IRQHandler,         /* 29: TIM3                          */
    TIM4_IRQHandler,         /* 30: TIM4                          */
    I2C1_EV_IRQHandler,      /* 31: I2C1 Event                   */
    I2C1_ER_IRQHandler,      /* 32: I2C1 Error                   */
    I2C2_EV_IRQHandler,      /* 33: I2C2 Event                   */
    I2C2_ER_IRQHandler,      /* 34: I2C2 Error                   */
    SPI1_IRQHandler,         /* 35: SPI1                          */
    SPI2_IRQHandler,         /* 36: SPI2                          */
    USART1_IRQHandler,       /* 37: USART1                        */
    USART2_IRQHandler,       /* 38: USART2                        */
    USART3_IRQHandler,       /* 39: USART3                        */
    EXTI15_10_IRQHandler,    /* 40: EXTI Lines 10–15              */
    RTC_Alarm_IRQHandler,    /* 41: RTC Alarm through EXTI        */
    USBWakeUp_IRQHandler,    /* 42: USB Wakeup                    */
};

/* ---------------------------------------------------------------------------
 * Reset_Handler — first code to run after power-on / reset
 * ------------------------------------------------------------------------ */
void Reset_Handler(void)
{
    uint32_t *src, *dst;

    /* Copy .data initializers from flash to SRAM */
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero-fill .bss */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* Call the application entry point */
    (void)main();

    /* If main() returns, hang */
    while (1) {
        __asm__ volatile("nop");
    }
}

/* ---------------------------------------------------------------------------
 * Default_Handler — catches any unhandled interrupt
 * ------------------------------------------------------------------------ */
void Default_Handler(void)
{
    while (1) {
        __asm__ volatile("nop");
    }
}
