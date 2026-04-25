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

/* AHB */
#define DMA1_BASE       0x40020000UL

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

/* RCC_AHBENR bit positions */
#define RCC_AHBENR_DMA1EN  (1UL << 0)

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
 * USART — Universal Synchronous/Asynchronous Receiver/Transmitter
 * ------------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t SR;           /* 0x00: Status                      */
    volatile uint32_t DR;           /* 0x04: Data                        */
    volatile uint32_t BRR;          /* 0x08: Baud rate                   */
    volatile uint32_t CR1;          /* 0x0C: Control 1                   */
    volatile uint32_t CR2;          /* 0x10: Control 2                   */
    volatile uint32_t CR3;          /* 0x14: Control 3                   */
    volatile uint32_t GTPR;         /* 0x18: Guard time and prescaler    */
} USART_TypeDef;

#define USART1  ((USART_TypeDef *)USART1_BASE)
#define USART2  ((USART_TypeDef *)USART2_BASE)
#define USART3  ((USART_TypeDef *)USART3_BASE)

/* USART_SR bit positions */
#define USART_SR_PE         (1UL << 0)      /* Parity error              */
#define USART_SR_FE         (1UL << 1)      /* Framing error             */
#define USART_SR_NE         (1UL << 2)      /* Noise error               */
#define USART_SR_ORE        (1UL << 3)      /* Overrun error             */
#define USART_SR_IDLE       (1UL << 4)      /* IDLE line detected        */
#define USART_SR_RXNE       (1UL << 5)      /* Read data register not empty */
#define USART_SR_TC         (1UL << 6)      /* Transmission complete     */
#define USART_SR_TXE        (1UL << 7)      /* Transmit data reg empty   */
#define USART_SR_LBD        (1UL << 8)      /* LIN break detection       */
#define USART_SR_CTS        (1UL << 9)      /* CTS flag                  */

/* USART_CR1 bit positions */
#define USART_CR1_SBK       (1UL << 0)      /* Send break                */
#define USART_CR1_RWU       (1UL << 1)      /* Receiver wakeup           */
#define USART_CR1_RE        (1UL << 2)      /* Receiver enable           */
#define USART_CR1_TE        (1UL << 3)      /* Transmitter enable        */
#define USART_CR1_IDLEIE    (1UL << 4)      /* IDLE interrupt enable     */
#define USART_CR1_RXNEIE    (1UL << 5)      /* RXNE interrupt enable     */
#define USART_CR1_TCIE      (1UL << 6)      /* TC interrupt enable       */
#define USART_CR1_TXEIE     (1UL << 7)      /* TXE interrupt enable      */
#define USART_CR1_PEIE      (1UL << 8)      /* PE interrupt enable       */
#define USART_CR1_PS        (1UL << 9)      /* Parity selection (0=even) */
#define USART_CR1_PCE       (1UL << 10)     /* Parity control enable     */
#define USART_CR1_WAKE      (1UL << 11)     /* Wakeup method             */
#define USART_CR1_M         (1UL << 12)     /* Word length (0=8, 1=9)    */
#define USART_CR1_UE        (1UL << 13)     /* USART enable              */

/* USART_CR2 bit positions */
#define USART_CR2_STOP_MASK (3UL << 12)     /* STOP bits mask            */
#define USART_CR2_STOP_1    (0UL << 12)     /* 1 stop bit                */
#define USART_CR2_STOP_0_5  (1UL << 12)     /* 0.5 stop bits             */
#define USART_CR2_STOP_2    (2UL << 12)     /* 2 stop bits               */
#define USART_CR2_STOP_1_5  (3UL << 12)     /* 1.5 stop bits             */

/* USART_CR3 bit positions */
#define USART_CR3_EIE       (1UL << 0)      /* Error interrupt enable    */
#define USART_CR3_DMAT      (1UL << 7)      /* DMA enable transmitter    */
#define USART_CR3_DMAR      (1UL << 6)      /* DMA enable receiver       */

/* ---------------------------------------------------------------------------
 * DMA — Direct Memory Access Controller
 * ------------------------------------------------------------------------ */

/** Per-channel register block (7 channels on DMA1) */
typedef struct {
    volatile uint32_t CCR;          /* 0x00: Channel configuration       */
    volatile uint32_t CNDTR;        /* 0x04: Number of data to transfer  */
    volatile uint32_t CPAR;         /* 0x08: Peripheral address          */
    volatile uint32_t CMAR;         /* 0x0C: Memory address              */
    uint32_t RESERVED;              /* 0x10: (padding to 0x14 stride)    */
} DMA_Channel_TypeDef;

/** DMA controller registers */
typedef struct {
    volatile uint32_t ISR;          /* 0x00: Interrupt status            */
    volatile uint32_t IFCR;         /* 0x04: Interrupt flag clear        */
    DMA_Channel_TypeDef CH[7];      /* 0x08: Channels 1–7 (index 0–6)   */
} DMA_TypeDef;

#define DMA1    ((DMA_TypeDef *)DMA1_BASE)

/* DMA_ISR / DMA_IFCR bit offsets per channel (ch = 0..6 for channels 1..7) */
#define DMA_ISR_GIF(ch)     (1UL << ((ch) * 4 + 0))    /* Global interrupt  */
#define DMA_ISR_TCIF(ch)    (1UL << ((ch) * 4 + 1))    /* Transfer complete */
#define DMA_ISR_HTIF(ch)    (1UL << ((ch) * 4 + 2))    /* Half transfer     */
#define DMA_ISR_TEIF(ch)    (1UL << ((ch) * 4 + 3))    /* Transfer error    */

/* DMA_CCR bit positions */
#define DMA_CCR_EN          (1UL << 0)      /* Channel enable            */
#define DMA_CCR_TCIE        (1UL << 1)      /* Transfer complete IE      */
#define DMA_CCR_HTIE        (1UL << 2)      /* Half transfer IE          */
#define DMA_CCR_TEIE        (1UL << 3)      /* Transfer error IE         */
#define DMA_CCR_DIR         (1UL << 4)      /* Direction (1=mem→periph)  */
#define DMA_CCR_CIRC        (1UL << 5)      /* Circular mode             */
#define DMA_CCR_PINC        (1UL << 6)      /* Peripheral increment      */
#define DMA_CCR_MINC        (1UL << 7)      /* Memory increment          */
#define DMA_CCR_PSIZE_MASK  (3UL << 8)      /* Peripheral data size      */
#define DMA_CCR_PSIZE_8     (0UL << 8)      /* 8-bit peripheral          */
#define DMA_CCR_PSIZE_16    (1UL << 8)      /* 16-bit peripheral         */
#define DMA_CCR_PSIZE_32    (2UL << 8)      /* 32-bit peripheral         */
#define DMA_CCR_MSIZE_MASK  (3UL << 10)     /* Memory data size          */
#define DMA_CCR_MSIZE_8     (0UL << 10)     /* 8-bit memory              */
#define DMA_CCR_MSIZE_16    (1UL << 10)     /* 16-bit memory             */
#define DMA_CCR_MSIZE_32    (2UL << 10)     /* 32-bit memory             */
#define DMA_CCR_PL_MASK     (3UL << 12)     /* Priority level            */
#define DMA_CCR_PL_LOW      (0UL << 12)
#define DMA_CCR_PL_MEDIUM   (1UL << 12)
#define DMA_CCR_PL_HIGH     (2UL << 12)
#define DMA_CCR_PL_VERY_HIGH (3UL << 12)
#define DMA_CCR_MEM2MEM     (1UL << 14)     /* Memory-to-memory mode     */

/* ---------------------------------------------------------------------------
 * SPI — Serial Peripheral Interface
 * ------------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t CR1;          /* 0x00: Control register 1          */
    volatile uint32_t CR2;          /* 0x04: Control register 2          */
    volatile uint32_t SR;           /* 0x08: Status register             */
    volatile uint32_t DR;           /* 0x0C: Data register               */
    volatile uint32_t CRCPR;        /* 0x10: CRC polynomial              */
    volatile uint32_t RXCRCR;       /* 0x14: RX CRC                     */
    volatile uint32_t TXCRCR;       /* 0x18: TX CRC                     */
} SPI_TypeDef;

#define SPI1    ((SPI_TypeDef *)SPI1_BASE)
#define SPI2    ((SPI_TypeDef *)SPI2_BASE)

/* SPI_CR1 bit positions */
#define SPI_CR1_CPHA        (1UL << 0)      /* Clock phase               */
#define SPI_CR1_CPOL        (1UL << 1)      /* Clock polarity            */
#define SPI_CR1_MSTR        (1UL << 2)      /* Master selection          */
#define SPI_CR1_BR_MASK     (7UL << 3)      /* Baud rate prescaler mask  */
#define SPI_CR1_BR_DIV2     (0UL << 3)      /* f_pclk / 2               */
#define SPI_CR1_BR_DIV4     (1UL << 3)      /* f_pclk / 4               */
#define SPI_CR1_BR_DIV8     (2UL << 3)      /* f_pclk / 8               */
#define SPI_CR1_BR_DIV16    (3UL << 3)      /* f_pclk / 16              */
#define SPI_CR1_BR_DIV32    (4UL << 3)      /* f_pclk / 32              */
#define SPI_CR1_BR_DIV64    (5UL << 3)      /* f_pclk / 64              */
#define SPI_CR1_BR_DIV128   (6UL << 3)      /* f_pclk / 128             */
#define SPI_CR1_BR_DIV256   (7UL << 3)      /* f_pclk / 256             */
#define SPI_CR1_SPE         (1UL << 6)      /* SPI enable                */
#define SPI_CR1_LSBFIRST    (1UL << 7)      /* Frame format (1=LSB first)*/
#define SPI_CR1_SSI         (1UL << 8)      /* Internal slave select     */
#define SPI_CR1_SSM         (1UL << 9)      /* Software slave management */
#define SPI_CR1_RXONLY      (1UL << 10)     /* Receive only              */
#define SPI_CR1_DFF         (1UL << 11)     /* Data frame format (1=16b) */
#define SPI_CR1_CRCNEXT     (1UL << 12)     /* Transmit CRC next         */
#define SPI_CR1_CRCEN       (1UL << 13)     /* CRC calculation enable    */
#define SPI_CR1_BIDIOE      (1UL << 14)     /* Output enable in bidi     */
#define SPI_CR1_BIDIMODE    (1UL << 15)     /* Bidirectional data mode   */

/* SPI_CR2 bit positions */
#define SPI_CR2_RXDMAEN     (1UL << 0)      /* RX buffer DMA enable      */
#define SPI_CR2_TXDMAEN     (1UL << 1)      /* TX buffer DMA enable      */
#define SPI_CR2_SSOE        (1UL << 2)      /* SS output enable          */
#define SPI_CR2_ERRIE       (1UL << 5)      /* Error interrupt enable    */
#define SPI_CR2_RXNEIE      (1UL << 6)      /* RX not empty IE           */
#define SPI_CR2_TXEIE       (1UL << 7)      /* TX empty IE               */

/* SPI_SR bit positions */
#define SPI_SR_RXNE         (1UL << 0)      /* Receive buffer not empty  */
#define SPI_SR_TXE          (1UL << 1)      /* Transmit buffer empty     */
#define SPI_SR_CHSIDE       (1UL << 2)      /* Channel side              */
#define SPI_SR_UDR          (1UL << 3)      /* Underrun flag             */
#define SPI_SR_CRCERR       (1UL << 4)      /* CRC error flag            */
#define SPI_SR_MODF         (1UL << 5)      /* Mode fault                */
#define SPI_SR_OVR          (1UL << 6)      /* Overrun flag              */
#define SPI_SR_BSY          (1UL << 7)      /* Busy flag                 */

/* ---------------------------------------------------------------------------
 * I2C — Inter-Integrated Circuit Interface
 * ------------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t CR1;          /* 0x00: Control register 1          */
    volatile uint32_t CR2;          /* 0x04: Control register 2          */
    volatile uint32_t OAR1;         /* 0x08: Own address register 1      */
    volatile uint32_t OAR2;         /* 0x0C: Own address register 2      */
    volatile uint32_t DR;           /* 0x10: Data register               */
    volatile uint32_t SR1;          /* 0x14: Status register 1           */
    volatile uint32_t SR2;          /* 0x18: Status register 2           */
    volatile uint32_t CCR;          /* 0x1C: Clock control register      */
    volatile uint32_t TRISE;        /* 0x20: Rise time register          */
} I2C_TypeDef;

#define I2C1    ((I2C_TypeDef *)I2C1_BASE)
#define I2C2    ((I2C_TypeDef *)I2C2_BASE)

/* I2C_CR1 bit positions */
#define I2C_CR1_PE          (1UL << 0)      /* Peripheral enable         */
#define I2C_CR1_SMBUS       (1UL << 1)      /* SMBus mode                */
#define I2C_CR1_SMBTYPE     (1UL << 3)      /* SMBus type                */
#define I2C_CR1_ENARP       (1UL << 4)      /* ARP enable                */
#define I2C_CR1_ENPEC       (1UL << 5)      /* PEC enable                */
#define I2C_CR1_ENGC        (1UL << 6)      /* General call enable       */
#define I2C_CR1_NOSTRETCH   (1UL << 7)      /* Clock stretching disable  */
#define I2C_CR1_START       (1UL << 8)      /* Start generation          */
#define I2C_CR1_STOP        (1UL << 9)      /* Stop generation           */
#define I2C_CR1_ACK         (1UL << 10)     /* Acknowledge enable        */
#define I2C_CR1_POS         (1UL << 11)     /* Ack/PEC position          */
#define I2C_CR1_PEC         (1UL << 12)     /* Packet error checking     */
#define I2C_CR1_ALERT       (1UL << 13)     /* SMBus alert               */
#define I2C_CR1_SWRST       (1UL << 15)     /* Software reset            */

/* I2C_CR2 bit positions */
#define I2C_CR2_FREQ_MASK   (0x3FUL)        /* Peripheral clock freq MHz */
#define I2C_CR2_ITERREN     (1UL << 8)      /* Error interrupt enable    */
#define I2C_CR2_ITEVTEN     (1UL << 9)      /* Event interrupt enable    */
#define I2C_CR2_ITBUFEN     (1UL << 10)     /* Buffer interrupt enable   */
#define I2C_CR2_DMAEN       (1UL << 11)     /* DMA requests enable       */
#define I2C_CR2_LAST        (1UL << 12)     /* DMA last transfer         */

/* I2C_SR1 bit positions */
#define I2C_SR1_SB          (1UL << 0)      /* Start bit (master)        */
#define I2C_SR1_ADDR        (1UL << 1)      /* Address sent/matched      */
#define I2C_SR1_BTF         (1UL << 2)      /* Byte transfer finished    */
#define I2C_SR1_ADD10       (1UL << 3)      /* 10-bit header sent        */
#define I2C_SR1_STOPF       (1UL << 4)      /* Stop detection (slave)    */
#define I2C_SR1_RXNE        (1UL << 6)      /* Data register not empty   */
#define I2C_SR1_TXE         (1UL << 7)      /* Data register empty       */
#define I2C_SR1_BERR        (1UL << 8)      /* Bus error                 */
#define I2C_SR1_ARLO        (1UL << 9)      /* Arbitration lost          */
#define I2C_SR1_AF          (1UL << 10)     /* Acknowledge failure       */
#define I2C_SR1_OVR         (1UL << 11)     /* Overrun/underrun          */
#define I2C_SR1_PECERR      (1UL << 12)     /* PEC error in reception    */
#define I2C_SR1_TIMEOUT     (1UL << 14)     /* Timeout or Tlow error     */
#define I2C_SR1_SMBALERT    (1UL << 15)     /* SMBus alert               */

/* I2C_SR2 bit positions */
#define I2C_SR2_MSL         (1UL << 0)      /* Master/slave              */
#define I2C_SR2_BUSY        (1UL << 1)      /* Bus busy                  */
#define I2C_SR2_TRA         (1UL << 2)      /* Transmitter/receiver      */
#define I2C_SR2_GENCALL     (1UL << 4)      /* General call address      */
#define I2C_SR2_DUALF       (1UL << 7)      /* Dual flag                 */

/* I2C_CCR bit positions */
#define I2C_CCR_CCR_MASK    (0xFFFUL)       /* Clock control value       */
#define I2C_CCR_DUTY        (1UL << 14)     /* Fast mode duty cycle      */
#define I2C_CCR_FS          (1UL << 15)     /* Master mode selection     */

/* I2C NVIC IRQ numbers (STM32F103) */
#define I2C1_EV_IRQn    31
#define I2C1_ER_IRQn    32
#define I2C2_EV_IRQn    33
#define I2C2_ER_IRQn    34

/* SPI NVIC IRQ numbers (STM32F103) */
#define SPI1_IRQn       35
#define SPI2_IRQn       36

/* USART NVIC IRQ numbers (STM32F103) */
#define USART1_IRQn     37
#define USART2_IRQn     38
#define USART3_IRQn     39

/* DMA1 channel NVIC IRQ numbers (STM32F103) */
#define DMA1_Channel1_IRQn  11
#define DMA1_Channel2_IRQn  12
#define DMA1_Channel3_IRQn  13
#define DMA1_Channel4_IRQn  14
#define DMA1_Channel5_IRQn  15
#define DMA1_Channel6_IRQn  16
#define DMA1_Channel7_IRQn  17

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
