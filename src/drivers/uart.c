/**
 * @file  uart.c
 * @brief UART driver implementation for STM32F1xx
 *
 * Provides polling, interrupt-driven, and DMA transfer modes for
 * USART1, USART2, and USART3. Each instance is managed through a
 * uart_handle_t that the caller allocates and passes to uart_init().
 *
 * DMA channel assignments (STM32F103 reference manual Table 78):
 *   USART1_TX → DMA1 Channel 4   (index 3, IRQ 14)
 *   USART1_RX → DMA1 Channel 5   (index 4, IRQ 15)
 *   USART2_TX → DMA1 Channel 7   (index 6, IRQ 17)
 *   USART2_RX → DMA1 Channel 6   (index 5, IRQ 16)
 *   USART3_TX → DMA1 Channel 2   (index 1, IRQ 12)
 *   USART3_RX → DMA1 Channel 3   (index 2, IRQ 13)
 */

#include "drivers/uart.h"

/* ── Ring-buffer helpers ───────────────────────────────────────────────── */

static inline uint16_t ring_next(uint16_t idx)
{
    return (uint16_t)((idx + 1U) % UART_RING_BUF_SIZE);
}

static inline bool ring_is_empty(const uart_ring_buf_t *rb)
{
    return rb->head == rb->tail;
}

static inline bool ring_is_full(const uart_ring_buf_t *rb)
{
    return ring_next(rb->head) == rb->tail;
}

static inline bool ring_put(uart_ring_buf_t *rb, uint8_t byte)
{
    if (ring_is_full(rb)) {
        return false;
    }
    rb->buf[rb->head] = byte;
    rb->head = ring_next(rb->head);
    return true;
}

static inline bool ring_get(uart_ring_buf_t *rb, uint8_t *byte)
{
    if (ring_is_empty(rb)) {
        return false;
    }
    *byte = rb->buf[rb->tail];
    rb->tail = ring_next(rb->tail);
    return true;
}

static inline size_t ring_count(const uart_ring_buf_t *rb)
{
    int32_t diff = (int32_t)rb->head - (int32_t)rb->tail;
    if (diff < 0) {
        diff += UART_RING_BUF_SIZE;
    }
    return (size_t)diff;
}

static void ring_reset(uart_ring_buf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/* ── Internal helpers ──────────────────────────────────────────────────── */

/**
 * @brief Return the APB bus clock frequency for a given USART
 *
 * USART1 is on APB2 (default 72 MHz), USART2/3 on APB1 (default 36 MHz).
 * These values assume the default clock tree coming out of reset with an
 * 8 MHz HSE crystal and the PLL configured for 72 MHz SYSCLK. Override
 * if your clock setup differs.
 */
static uint32_t uart_get_pclk(const USART_TypeDef *usart)
{
    if (usart == USART1) {
        return 72000000UL;  /* APB2 clock */
    }
    return 36000000UL;      /* APB1 clock */
}

/**
 * @brief Enable the peripheral clock for the specified USART
 */
static void uart_clock_enable(const USART_TypeDef *usart)
{
    if (usart == USART1) {
        REG_SET(RCC->APB2ENR, RCC_APB2ENR_USART1EN);
    } else if (usart == USART2) {
        REG_SET(RCC->APB1ENR, RCC_APB1ENR_USART2EN);
    } else if (usart == USART3) {
        REG_SET(RCC->APB1ENR, RCC_APB1ENR_USART3EN);
    }
}

/**
 * @brief Disable the peripheral clock for the specified USART
 */
static void uart_clock_disable(const USART_TypeDef *usart)
{
    if (usart == USART1) {
        REG_CLR(RCC->APB2ENR, RCC_APB2ENR_USART1EN);
    } else if (usart == USART2) {
        REG_CLR(RCC->APB1ENR, RCC_APB1ENR_USART2EN);
    } else if (usart == USART3) {
        REG_CLR(RCC->APB1ENR, RCC_APB1ENR_USART3EN);
    }
}

/**
 * @brief Return the NVIC IRQ number for a given USART instance
 */
static uint32_t uart_irqn(const USART_TypeDef *usart)
{
    if (usart == USART1) return USART1_IRQn;
    if (usart == USART2) return USART2_IRQn;
    return USART3_IRQn;
}

/**
 * @brief Enable an IRQ in the NVIC
 */
static inline void nvic_enable_irq(uint32_t irqn)
{
    NVIC->ISER[irqn / 32U] = (1UL << (irqn % 32U));
}

/**
 * @brief Disable an IRQ in the NVIC
 */
static inline void nvic_disable_irq(uint32_t irqn)
{
    NVIC->ICER[irqn / 32U] = (1UL << (irqn % 32U));
}

/* ── Initialisation / deinitialisation ─────────────────────────────────── */

void uart_init(uart_handle_t *handle, USART_TypeDef *usart,
               const uart_config_t *config)
{
    handle->instance = usart;
    handle->config   = *config;
    handle->state    = UART_STATE_RESET;
    handle->errors   = UART_ERROR_NONE;

    handle->on_tx_complete = (void *)0;
    handle->on_rx_complete = (void *)0;
    handle->on_error       = (void *)0;

    handle->dma_tx.channel = (void *)0;
    handle->dma_rx.channel = (void *)0;

    ring_reset(&handle->tx_ring);
    ring_reset(&handle->rx_ring);

    /* Enable peripheral clock */
    uart_clock_enable(usart);

    /* Disable USART while configuring */
    REG_CLR(usart->CR1, USART_CR1_UE);

    /*
     * Baud rate register (BRR):
     *   USARTDIV = f_pclk / (16 * baud)
     *   BRR = mantissa[15:4] | fraction[3:0]
     *
     * We compute with fixed-point rounding:
     *   USARTDIV * 16 = f_pclk / baud  (then split into mantissa + frac)
     */
    uint32_t pclk = uart_get_pclk(usart);
    uint32_t div16 = (pclk + config->baud_rate / 2U) / config->baud_rate;
    uint32_t mantissa = div16 / 16U;
    uint32_t fraction = div16 % 16U;
    usart->BRR = (mantissa << 4) | fraction;

    /* Word length */
    if (config->word_length == UART_WORD_9BIT) {
        REG_SET(usart->CR1, USART_CR1_M);
    } else {
        REG_CLR(usart->CR1, USART_CR1_M);
    }

    /* Parity */
    if (config->parity == UART_PARITY_NONE) {
        REG_CLR(usart->CR1, USART_CR1_PCE);
    } else {
        REG_SET(usart->CR1, USART_CR1_PCE);
        if (config->parity == UART_PARITY_ODD) {
            REG_SET(usart->CR1, USART_CR1_PS);
        } else {
            REG_CLR(usart->CR1, USART_CR1_PS);
        }
    }

    /* Stop bits */
    REG_MODIFY(usart->CR2, USART_CR2_STOP_MASK,
               (uint32_t)config->stop_bits << 12);

    /* Direction */
    uint32_t dir_bits = 0;
    if (config->direction & UART_DIR_TX) dir_bits |= USART_CR1_TE;
    if (config->direction & UART_DIR_RX) dir_bits |= USART_CR1_RE;
    REG_MODIFY(usart->CR1, USART_CR1_TE | USART_CR1_RE, dir_bits);

    /* Enable USART */
    REG_SET(usart->CR1, USART_CR1_UE);

    handle->state = UART_STATE_READY;
}

void uart_deinit(uart_handle_t *handle)
{
    USART_TypeDef *usart = handle->instance;

    /* Disable all interrupts and DMA requests */
    usart->CR1 &= ~(USART_CR1_TXEIE | USART_CR1_RXNEIE |
                     USART_CR1_TCIE | USART_CR1_PEIE | USART_CR1_IDLEIE);
    usart->CR3 &= ~(USART_CR3_DMAT | USART_CR3_DMAR | USART_CR3_EIE);

    /* Disable USART */
    REG_CLR(usart->CR1, USART_CR1_UE);

    /* Disable NVIC IRQ */
    nvic_disable_irq(uart_irqn(usart));

    /* Stop any active DMA channels */
    if (handle->dma_tx.channel) {
        REG_CLR(handle->dma_tx.channel->CCR, DMA_CCR_EN);
        nvic_disable_irq(handle->dma_tx.irqn);
    }
    if (handle->dma_rx.channel) {
        REG_CLR(handle->dma_rx.channel->CCR, DMA_CCR_EN);
        nvic_disable_irq(handle->dma_rx.irqn);
    }

    /* Disable peripheral clock */
    uart_clock_disable(usart);

    handle->state = UART_STATE_RESET;
}

/* ── Polling (blocking) transfers ──────────────────────────────────────── */

void uart_transmit(uart_handle_t *handle, const uint8_t *data, size_t len)
{
    USART_TypeDef *usart = handle->instance;

    for (size_t i = 0; i < len; i++) {
        /* Wait until the transmit data register is empty */
        while (!(usart->SR & USART_SR_TXE)) {
            /* spin */
        }
        usart->DR = data[i];
    }

    /* Wait for transmission of the last byte to complete */
    while (!(usart->SR & USART_SR_TC)) {
        /* spin */
    }
}

void uart_receive(uart_handle_t *handle, uint8_t *data, size_t len)
{
    USART_TypeDef *usart = handle->instance;

    for (size_t i = 0; i < len; i++) {
        /* Wait until data is available */
        while (!(usart->SR & USART_SR_RXNE)) {
            /* spin */
        }
        data[i] = (uint8_t)(usart->DR & 0xFFU);
    }
}

/* ── Interrupt-driven transfers ────────────────────────────────────────── */

size_t uart_transmit_it(uart_handle_t *handle, const uint8_t *data,
                        size_t len)
{
    size_t queued = 0;

    for (size_t i = 0; i < len; i++) {
        if (!ring_put(&handle->tx_ring, data[i])) {
            break;  /* Ring buffer full */
        }
        queued++;
    }

    if (queued > 0) {
        /* Mark TX active */
        if (handle->state == UART_STATE_BUSY_RX) {
            handle->state = UART_STATE_BUSY_TX_RX;
        } else if (handle->state != UART_STATE_BUSY_TX_RX) {
            handle->state = UART_STATE_BUSY_TX;
        }

        /* Enable USART IRQ in NVIC and turn on TXE interrupt */
        nvic_enable_irq(uart_irqn(handle->instance));
        REG_SET(handle->instance->CR1, USART_CR1_TXEIE);
    }

    return queued;
}

void uart_receive_it(uart_handle_t *handle)
{
    if (handle->state == UART_STATE_BUSY_TX) {
        handle->state = UART_STATE_BUSY_TX_RX;
    } else if (handle->state != UART_STATE_BUSY_TX_RX) {
        handle->state = UART_STATE_BUSY_RX;
    }

    nvic_enable_irq(uart_irqn(handle->instance));

    /* Enable RXNE and error interrupts */
    REG_SET(handle->instance->CR1, USART_CR1_RXNEIE | USART_CR1_PEIE);
    REG_SET(handle->instance->CR3, USART_CR3_EIE);
}

void uart_stop_receive_it(uart_handle_t *handle)
{
    REG_CLR(handle->instance->CR1, USART_CR1_RXNEIE | USART_CR1_PEIE);
    REG_CLR(handle->instance->CR3, USART_CR3_EIE);

    if (handle->state == UART_STATE_BUSY_TX_RX) {
        handle->state = UART_STATE_BUSY_TX;
    } else if (handle->state == UART_STATE_BUSY_RX) {
        handle->state = UART_STATE_READY;
    }
}

size_t uart_read_rx_buffer(uart_handle_t *handle, uint8_t *data,
                           size_t max_len)
{
    size_t count = 0;
    uint8_t byte;

    while (count < max_len && ring_get(&handle->rx_ring, &byte)) {
        data[count++] = byte;
    }
    return count;
}

size_t uart_rx_available(const uart_handle_t *handle)
{
    return ring_count(&handle->rx_ring);
}

/* ── DMA transfers ─────────────────────────────────────────────────────── */

void uart_config_dma(uart_handle_t *handle,
                     DMA_Channel_TypeDef *tx_channel, uint8_t tx_index,
                     uint8_t tx_irqn,
                     DMA_Channel_TypeDef *rx_channel, uint8_t rx_index,
                     uint8_t rx_irqn)
{
    /* Enable DMA1 clock */
    REG_SET(RCC->AHBENR, RCC_AHBENR_DMA1EN);

    handle->dma_tx.channel  = tx_channel;
    handle->dma_tx.ch_index = tx_index;
    handle->dma_tx.irqn     = tx_irqn;
    handle->dma_tx.busy     = false;

    handle->dma_rx.channel  = rx_channel;
    handle->dma_rx.ch_index = rx_index;
    handle->dma_rx.irqn     = rx_irqn;
    handle->dma_rx.busy     = false;
}

int uart_transmit_dma(uart_handle_t *handle, const uint8_t *data,
                      size_t len)
{
    if (!handle->dma_tx.channel || handle->dma_tx.busy) {
        return -1;
    }

    DMA_Channel_TypeDef *ch = handle->dma_tx.channel;
    handle->dma_tx.busy = true;

    /* Disable channel before configuration */
    REG_CLR(ch->CCR, DMA_CCR_EN);

    /* Clear any pending flags for this channel */
    DMA1->IFCR = DMA_ISR_GIF(handle->dma_tx.ch_index);

    /* Configure: memory→peripheral, 8-bit, memory increment, TC interrupt */
    ch->CCR = DMA_CCR_DIR        /* Memory → peripheral */
            | DMA_CCR_MINC       /* Increment memory address */
            | DMA_CCR_PSIZE_8    /* Peripheral size: byte */
            | DMA_CCR_MSIZE_8    /* Memory size: byte */
            | DMA_CCR_PL_MEDIUM  /* Priority: medium */
            | DMA_CCR_TCIE       /* Transfer complete interrupt */
            | DMA_CCR_TEIE;      /* Transfer error interrupt */

    ch->CNDTR = (uint32_t)len;
    ch->CPAR  = (uint32_t)&handle->instance->DR;
    ch->CMAR  = (uint32_t)data;

    /* Enable DMA request in USART */
    REG_SET(handle->instance->CR3, USART_CR3_DMAT);

    /* Enable the DMA channel NVIC IRQ and start the transfer */
    nvic_enable_irq(handle->dma_tx.irqn);
    REG_SET(ch->CCR, DMA_CCR_EN);

    return 0;
}

int uart_receive_dma(uart_handle_t *handle, uint8_t *data, size_t len)
{
    if (!handle->dma_rx.channel || handle->dma_rx.busy) {
        return -1;
    }

    DMA_Channel_TypeDef *ch = handle->dma_rx.channel;
    handle->dma_rx.busy = true;

    /* Disable channel before configuration */
    REG_CLR(ch->CCR, DMA_CCR_EN);

    /* Clear any pending flags */
    DMA1->IFCR = DMA_ISR_GIF(handle->dma_rx.ch_index);

    /* Configure: peripheral→memory, 8-bit, memory increment, TC interrupt */
    ch->CCR = DMA_CCR_MINC       /* Increment memory address */
            | DMA_CCR_PSIZE_8    /* Peripheral size: byte */
            | DMA_CCR_MSIZE_8    /* Memory size: byte */
            | DMA_CCR_PL_HIGH    /* Priority: high (RX is time-critical) */
            | DMA_CCR_TCIE       /* Transfer complete interrupt */
            | DMA_CCR_TEIE;      /* Transfer error interrupt */
    /* DIR=0: peripheral → memory (read from USART DR) */

    ch->CNDTR = (uint32_t)len;
    ch->CPAR  = (uint32_t)&handle->instance->DR;
    ch->CMAR  = (uint32_t)data;

    /* Enable DMA request in USART and error interrupt for framing/parity */
    REG_SET(handle->instance->CR3, USART_CR3_DMAR);
    REG_SET(handle->instance->CR3, USART_CR3_EIE);

    /* Enable the DMA channel NVIC IRQ and start the transfer */
    nvic_enable_irq(handle->dma_rx.irqn);
    REG_SET(ch->CCR, DMA_CCR_EN);

    return 0;
}

/* ── ISR entry points ──────────────────────────────────────────────────── */

/**
 * @brief Handle error flags in the USART status register
 *
 * Reads SR then DR to clear the error flags (as required by hardware),
 * accumulates error bits in handle->errors, and fires the error callback.
 */
static void uart_handle_errors(uart_handle_t *handle, uint32_t sr)
{
    /* Reading SR followed by DR clears PE, FE, NE, ORE */
    (void)handle->instance->DR;

    if (sr & USART_SR_PE)  handle->errors |= UART_ERROR_PARITY;
    if (sr & USART_SR_FE)  handle->errors |= UART_ERROR_FRAMING;
    if (sr & USART_SR_NE)  handle->errors |= UART_ERROR_NOISE;
    if (sr & USART_SR_ORE) handle->errors |= UART_ERROR_OVERRUN;

    if (handle->on_error) {
        handle->on_error(handle);
    }
}

void uart_irq_handler(uart_handle_t *handle)
{
    USART_TypeDef *usart = handle->instance;
    uint32_t sr  = usart->SR;
    uint32_t cr1 = usart->CR1;

    /* ── Error detection ─────────────────────────────────────────────── */
    if (sr & (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)) {
        uart_handle_errors(handle, sr);
        /* Re-read SR after clearing errors; flags may have changed */
        sr = usart->SR;
    }

    /* ── RXNE: received byte ready ───────────────────────────────────── */
    if ((sr & USART_SR_RXNE) && (cr1 & USART_CR1_RXNEIE)) {
        uint8_t byte = (uint8_t)(usart->DR & 0xFFU);
        ring_put(&handle->rx_ring, byte);
    }

    /* ── TXE: transmit register empty, feed next byte ────────────────── */
    if ((sr & USART_SR_TXE) && (cr1 & USART_CR1_TXEIE)) {
        uint8_t byte;
        if (ring_get(&handle->tx_ring, &byte)) {
            usart->DR = byte;
        } else {
            /*
             * Ring buffer drained — disable TXE interrupt and enable TC
             * so we get notified when the shift register finishes.
             */
            REG_CLR(usart->CR1, USART_CR1_TXEIE);
            REG_SET(usart->CR1, USART_CR1_TCIE);
        }
    }

    /* ── TC: transmission complete (last byte shifted out) ───────────── */
    if ((sr & USART_SR_TC) && (cr1 & USART_CR1_TCIE)) {
        REG_CLR(usart->CR1, USART_CR1_TCIE);

        /* Update state */
        if (handle->state == UART_STATE_BUSY_TX_RX) {
            handle->state = UART_STATE_BUSY_RX;
        } else if (handle->state == UART_STATE_BUSY_TX) {
            handle->state = UART_STATE_READY;
        }

        if (handle->on_tx_complete) {
            handle->on_tx_complete(handle);
        }
    }
}

void uart_dma_tx_handler(uart_handle_t *handle)
{
    uint8_t idx = handle->dma_tx.ch_index;
    uint32_t isr = DMA1->ISR;

    if (isr & DMA_ISR_TEIF(idx)) {
        /* DMA transfer error */
        DMA1->IFCR = DMA_ISR_TEIF(idx);
        handle->errors |= UART_ERROR_DMA;
        if (handle->on_error) {
            handle->on_error(handle);
        }
    }

    if (isr & DMA_ISR_TCIF(idx)) {
        /* Transfer complete */
        DMA1->IFCR = DMA_ISR_TCIF(idx);

        /* Disable DMA channel and USART DMA request */
        REG_CLR(handle->dma_tx.channel->CCR, DMA_CCR_EN);
        REG_CLR(handle->instance->CR3, USART_CR3_DMAT);

        handle->dma_tx.busy = false;

        if (handle->on_tx_complete) {
            handle->on_tx_complete(handle);
        }
    }

    /* Clear any remaining flags (half-transfer, global) */
    DMA1->IFCR = DMA_ISR_GIF(idx);
}

void uart_dma_rx_handler(uart_handle_t *handle)
{
    uint8_t idx = handle->dma_rx.ch_index;
    uint32_t isr = DMA1->ISR;

    if (isr & DMA_ISR_TEIF(idx)) {
        /* DMA transfer error */
        DMA1->IFCR = DMA_ISR_TEIF(idx);
        handle->errors |= UART_ERROR_DMA;
        if (handle->on_error) {
            handle->on_error(handle);
        }
    }

    if (isr & DMA_ISR_TCIF(idx)) {
        /* Transfer complete */
        DMA1->IFCR = DMA_ISR_TCIF(idx);

        /* Disable DMA channel and USART DMA request */
        REG_CLR(handle->dma_rx.channel->CCR, DMA_CCR_EN);
        REG_CLR(handle->instance->CR3, USART_CR3_DMAR);

        handle->dma_rx.busy = false;

        if (handle->on_rx_complete) {
            handle->on_rx_complete(handle);
        }
    }

    /* Clear any remaining flags */
    DMA1->IFCR = DMA_ISR_GIF(idx);
}
