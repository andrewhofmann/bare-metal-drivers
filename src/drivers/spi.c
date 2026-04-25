/**
 * @file  spi.c
 * @brief SPI driver implementation for STM32F1xx
 *
 * Provides polling, interrupt-driven, and DMA transfer modes for
 * SPI1 and SPI2. Each instance is managed through an spi_handle_t
 * that the caller allocates and passes to spi_init().
 *
 * DMA channel assignments (STM32F103 reference manual Table 78):
 *   SPI1_TX → DMA1 Channel 3   (index 2, IRQ 13)
 *   SPI1_RX → DMA1 Channel 2   (index 1, IRQ 12)
 *   SPI2_TX → DMA1 Channel 5   (index 4, IRQ 15)
 *   SPI2_RX → DMA1 Channel 4   (index 3, IRQ 14)
 */

#include "drivers/spi.h"

/* ── Ring-buffer helpers ───────────────────────────────────────────────── */

static inline uint16_t ring_next(uint16_t idx)
{
    return (uint16_t)((idx + 1U) % SPI_RING_BUF_SIZE);
}

static inline bool ring_is_empty(const spi_ring_buf_t *rb)
{
    return rb->head == rb->tail;
}

static inline bool ring_is_full(const spi_ring_buf_t *rb)
{
    return ring_next(rb->head) == rb->tail;
}

static inline bool ring_put(spi_ring_buf_t *rb, uint8_t byte)
{
    if (ring_is_full(rb)) {
        return false;
    }
    rb->buf[rb->head] = byte;
    rb->head = ring_next(rb->head);
    return true;
}

static inline bool ring_get(spi_ring_buf_t *rb, uint8_t *byte)
{
    if (ring_is_empty(rb)) {
        return false;
    }
    *byte = rb->buf[rb->tail];
    rb->tail = ring_next(rb->tail);
    return true;
}

static inline size_t ring_count(const spi_ring_buf_t *rb)
{
    int32_t diff = (int32_t)rb->head - (int32_t)rb->tail;
    if (diff < 0) {
        diff += SPI_RING_BUF_SIZE;
    }
    return (size_t)diff;
}

static void ring_reset(spi_ring_buf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/* ── Internal helpers ──────────────────────────────────────────────────── */

/**
 * @brief Enable the peripheral clock for the specified SPI
 *
 * SPI1 is on APB2, SPI2 is on APB1.
 */
static void spi_clock_enable(const SPI_TypeDef *spi)
{
    if (spi == SPI1) {
        REG_SET(RCC->APB2ENR, RCC_APB2ENR_SPI1EN);
    } else if (spi == SPI2) {
        REG_SET(RCC->APB1ENR, RCC_APB1ENR_SPI2EN);
    }
}

/**
 * @brief Disable the peripheral clock for the specified SPI
 */
static void spi_clock_disable(const SPI_TypeDef *spi)
{
    if (spi == SPI1) {
        REG_CLR(RCC->APB2ENR, RCC_APB2ENR_SPI1EN);
    } else if (spi == SPI2) {
        REG_CLR(RCC->APB1ENR, RCC_APB1ENR_SPI2EN);
    }
}

/**
 * @brief Return the NVIC IRQ number for a given SPI instance
 */
static uint32_t spi_irqn(const SPI_TypeDef *spi)
{
    if (spi == SPI1) return SPI1_IRQn;
    return SPI2_IRQn;
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

/**
 * @brief Dummy byte sent during receive-only transfers to generate clock
 */
#define SPI_DUMMY_BYTE  0xFFU

/* ── Initialisation / deinitialisation ─────────────────────────────────── */

void spi_init(spi_handle_t *handle, SPI_TypeDef *spi,
              const spi_config_t *config)
{
    handle->instance = spi;
    handle->config   = *config;
    handle->state    = SPI_STATE_RESET;
    handle->errors   = SPI_ERROR_NONE;

    handle->on_tx_complete    = (void *)0;
    handle->on_rx_complete    = (void *)0;
    handle->on_tx_rx_complete = (void *)0;
    handle->on_error          = (void *)0;

    handle->dma_tx.channel = (void *)0;
    handle->dma_rx.channel = (void *)0;

    handle->tx_remaining = 0;
    handle->rx_remaining = 0;

    ring_reset(&handle->tx_ring);
    ring_reset(&handle->rx_ring);

    /* Enable peripheral clock */
    spi_clock_enable(spi);

    /* Disable SPI while configuring */
    REG_CLR(spi->CR1, SPI_CR1_SPE);

    /*
     * Build CR1 value:
     *   - Clock mode (CPOL, CPHA) from spi_mode_t
     *   - Master/slave selection
     *   - Baud rate prescaler
     *   - Data frame format (8/16-bit)
     *   - Bit order (MSB/LSB first)
     *   - NSS management
     */
    uint32_t cr1 = 0;

    /* Clock mode — spi_mode_t directly encodes CPOL and CPHA bits */
    cr1 |= (uint32_t)config->mode;

    /* Master selection */
    if (config->role == SPI_ROLE_MASTER) {
        cr1 |= SPI_CR1_MSTR;
    }

    /* Baud rate prescaler */
    cr1 |= ((uint32_t)config->prescaler << 3);

    /* Data frame format */
    if (config->data_size == SPI_DATA_16BIT) {
        cr1 |= SPI_CR1_DFF;
    }

    /* Bit order */
    if (config->bit_order == SPI_LSB_FIRST) {
        cr1 |= SPI_CR1_LSBFIRST;
    }

    /* NSS management */
    if (config->nss == SPI_NSS_SOFT) {
        /*
         * Software NSS: set SSM and SSI so the internal NSS signal stays
         * high, preventing an inadvertent mode fault in master mode.
         */
        cr1 |= SPI_CR1_SSM | SPI_CR1_SSI;
    }

    spi->CR1 = cr1;

    /* Hardware NSS: enable SS output so the peripheral drives the pin */
    if (config->nss == SPI_NSS_HARD && config->role == SPI_ROLE_MASTER) {
        REG_SET(spi->CR2, SPI_CR2_SSOE);
    }

    /* Enable SPI */
    REG_SET(spi->CR1, SPI_CR1_SPE);

    handle->state = SPI_STATE_READY;
}

void spi_deinit(spi_handle_t *handle)
{
    SPI_TypeDef *spi = handle->instance;

    /* Disable all interrupts */
    spi->CR2 &= ~(SPI_CR2_TXEIE | SPI_CR2_RXNEIE | SPI_CR2_ERRIE);

    /* Wait for any ongoing transfer to finish */
    while (spi->SR & SPI_SR_BSY) {
        /* spin */
    }

    /* Disable SPI */
    REG_CLR(spi->CR1, SPI_CR1_SPE);

    /* Disable NVIC IRQ */
    nvic_disable_irq(spi_irqn(spi));

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
    spi_clock_disable(spi);

    handle->state = SPI_STATE_RESET;
}

/* ── Polling (blocking) transfers ──────────────────────────────────────── */

void spi_transmit(spi_handle_t *handle, const uint8_t *data, size_t len)
{
    SPI_TypeDef *spi = handle->instance;

    for (size_t i = 0; i < len; i++) {
        /* Wait until the transmit buffer is empty */
        while (!(spi->SR & SPI_SR_TXE)) {
            /* spin */
        }
        spi->DR = data[i];
    }

    /* Wait for the last byte to finish transmitting */
    while (spi->SR & SPI_SR_BSY) {
        /* spin */
    }

    /*
     * Clear any data left in the RX buffer by reading DR and SR.
     * The OVR flag is cleared by reading DR then SR.
     */
    (void)spi->DR;
    (void)spi->SR;
}

void spi_receive(spi_handle_t *handle, uint8_t *data, size_t len)
{
    SPI_TypeDef *spi = handle->instance;

    for (size_t i = 0; i < len; i++) {
        /* Wait for TX buffer empty, then send dummy to generate clock */
        while (!(spi->SR & SPI_SR_TXE)) {
            /* spin */
        }
        spi->DR = SPI_DUMMY_BYTE;

        /* Wait for received data */
        while (!(spi->SR & SPI_SR_RXNE)) {
            /* spin */
        }
        data[i] = (uint8_t)(spi->DR & 0xFFU);
    }

    while (spi->SR & SPI_SR_BSY) {
        /* spin */
    }
}

void spi_transmit_receive(spi_handle_t *handle, const uint8_t *tx_data,
                          uint8_t *rx_data, size_t len)
{
    SPI_TypeDef *spi = handle->instance;

    for (size_t i = 0; i < len; i++) {
        /* Wait for TX buffer empty */
        while (!(spi->SR & SPI_SR_TXE)) {
            /* spin */
        }
        spi->DR = tx_data[i];

        /* Wait for received data */
        while (!(spi->SR & SPI_SR_RXNE)) {
            /* spin */
        }
        rx_data[i] = (uint8_t)(spi->DR & 0xFFU);
    }

    while (spi->SR & SPI_SR_BSY) {
        /* spin */
    }
}

/* ── Interrupt-driven transfers ────────────────────────────────────────── */

size_t spi_transmit_it(spi_handle_t *handle, const uint8_t *data,
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
        handle->tx_remaining = (uint16_t)queued;
        handle->state = SPI_STATE_BUSY_TX;

        /* Enable SPI IRQ in NVIC and turn on TXE interrupt */
        nvic_enable_irq(spi_irqn(handle->instance));
        REG_SET(handle->instance->CR2, SPI_CR2_TXEIE);
    }

    return queued;
}

int spi_receive_it(spi_handle_t *handle, size_t len)
{
    if (handle->state != SPI_STATE_READY) {
        return -1;
    }

    handle->rx_remaining = (uint16_t)len;
    handle->state = SPI_STATE_BUSY_RX;

    nvic_enable_irq(spi_irqn(handle->instance));

    /*
     * In master mode the clock is only generated when we write to DR.
     * Send a dummy byte to kick off the first exchange; the ISR will
     * continue sending dummies for each remaining byte.
     */
    REG_SET(handle->instance->CR2, SPI_CR2_RXNEIE | SPI_CR2_ERRIE);

    if (handle->config.role == SPI_ROLE_MASTER) {
        while (!(handle->instance->SR & SPI_SR_TXE)) {
            /* spin — should be immediate after init */
        }
        handle->instance->DR = SPI_DUMMY_BYTE;
    }

    return 0;
}

size_t spi_read_rx_buffer(spi_handle_t *handle, uint8_t *data,
                          size_t max_len)
{
    size_t count = 0;
    uint8_t byte;

    while (count < max_len && ring_get(&handle->rx_ring, &byte)) {
        data[count++] = byte;
    }
    return count;
}

size_t spi_rx_available(const spi_handle_t *handle)
{
    return ring_count(&handle->rx_ring);
}

/* ── DMA transfers ─────────────────────────────────────────────────────── */

void spi_config_dma(spi_handle_t *handle,
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

int spi_transmit_dma(spi_handle_t *handle, const uint8_t *data,
                     size_t len)
{
    if (!handle->dma_tx.channel || handle->dma_tx.busy) {
        return -1;
    }

    DMA_Channel_TypeDef *ch = handle->dma_tx.channel;
    handle->dma_tx.busy = true;
    handle->state = SPI_STATE_BUSY_TX;

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

    /* Enable DMA request in SPI */
    REG_SET(handle->instance->CR2, SPI_CR2_TXDMAEN);

    /* Enable the DMA channel NVIC IRQ and start the transfer */
    nvic_enable_irq(handle->dma_tx.irqn);
    REG_SET(ch->CCR, DMA_CCR_EN);

    return 0;
}

int spi_receive_dma(spi_handle_t *handle, uint8_t *data, size_t len)
{
    if (!handle->dma_rx.channel || handle->dma_rx.busy) {
        return -1;
    }
    if (!handle->dma_tx.channel || handle->dma_tx.busy) {
        return -1;
    }

    handle->dma_rx.busy = true;
    handle->dma_tx.busy = true;
    handle->state = SPI_STATE_BUSY_RX;

    /*
     * Configure RX DMA channel: peripheral→memory, capturing incoming data.
     */
    DMA_Channel_TypeDef *rx_ch = handle->dma_rx.channel;
    REG_CLR(rx_ch->CCR, DMA_CCR_EN);
    DMA1->IFCR = DMA_ISR_GIF(handle->dma_rx.ch_index);

    rx_ch->CCR = DMA_CCR_MINC       /* Increment memory address */
               | DMA_CCR_PSIZE_8    /* Peripheral size: byte */
               | DMA_CCR_MSIZE_8    /* Memory size: byte */
               | DMA_CCR_PL_HIGH    /* Priority: high (RX is time-critical) */
               | DMA_CCR_TCIE       /* Transfer complete interrupt */
               | DMA_CCR_TEIE;      /* Transfer error interrupt */
    /* DIR=0: peripheral → memory */

    rx_ch->CNDTR = (uint32_t)len;
    rx_ch->CPAR  = (uint32_t)&handle->instance->DR;
    rx_ch->CMAR  = (uint32_t)data;

    /*
     * Configure TX DMA channel: sends dummy bytes to generate clock.
     * No memory increment — reads the same dummy byte repeatedly.
     */
    static const uint8_t dummy = SPI_DUMMY_BYTE;
    DMA_Channel_TypeDef *tx_ch = handle->dma_tx.channel;
    REG_CLR(tx_ch->CCR, DMA_CCR_EN);
    DMA1->IFCR = DMA_ISR_GIF(handle->dma_tx.ch_index);

    tx_ch->CCR = DMA_CCR_DIR        /* Memory → peripheral */
               | DMA_CCR_PSIZE_8
               | DMA_CCR_MSIZE_8
               | DMA_CCR_PL_LOW     /* Lower priority than RX */
               | DMA_CCR_TEIE;      /* Transfer error interrupt */
    /* MINC not set — always read from the same dummy address */

    tx_ch->CNDTR = (uint32_t)len;
    tx_ch->CPAR  = (uint32_t)&handle->instance->DR;
    tx_ch->CMAR  = (uint32_t)&dummy;

    /* Enable DMA requests in SPI (RX first to be ready before TX starts) */
    REG_SET(handle->instance->CR2, SPI_CR2_RXDMAEN);
    REG_SET(handle->instance->CR2, SPI_CR2_TXDMAEN);

    /* Enable NVIC IRQs and start both channels (RX before TX) */
    nvic_enable_irq(handle->dma_rx.irqn);
    nvic_enable_irq(handle->dma_tx.irqn);
    REG_SET(rx_ch->CCR, DMA_CCR_EN);
    REG_SET(tx_ch->CCR, DMA_CCR_EN);

    return 0;
}

int spi_transmit_receive_dma(spi_handle_t *handle, const uint8_t *tx_data,
                             uint8_t *rx_data, size_t len)
{
    if (!handle->dma_tx.channel || handle->dma_tx.busy) {
        return -1;
    }
    if (!handle->dma_rx.channel || handle->dma_rx.busy) {
        return -1;
    }

    handle->dma_tx.busy = true;
    handle->dma_rx.busy = true;
    handle->state = SPI_STATE_BUSY_TX_RX;

    /* Configure RX DMA channel */
    DMA_Channel_TypeDef *rx_ch = handle->dma_rx.channel;
    REG_CLR(rx_ch->CCR, DMA_CCR_EN);
    DMA1->IFCR = DMA_ISR_GIF(handle->dma_rx.ch_index);

    rx_ch->CCR = DMA_CCR_MINC
               | DMA_CCR_PSIZE_8
               | DMA_CCR_MSIZE_8
               | DMA_CCR_PL_HIGH
               | DMA_CCR_TCIE
               | DMA_CCR_TEIE;

    rx_ch->CNDTR = (uint32_t)len;
    rx_ch->CPAR  = (uint32_t)&handle->instance->DR;
    rx_ch->CMAR  = (uint32_t)rx_data;

    /* Configure TX DMA channel */
    DMA_Channel_TypeDef *tx_ch = handle->dma_tx.channel;
    REG_CLR(tx_ch->CCR, DMA_CCR_EN);
    DMA1->IFCR = DMA_ISR_GIF(handle->dma_tx.ch_index);

    tx_ch->CCR = DMA_CCR_DIR
               | DMA_CCR_MINC
               | DMA_CCR_PSIZE_8
               | DMA_CCR_MSIZE_8
               | DMA_CCR_PL_MEDIUM
               | DMA_CCR_TCIE
               | DMA_CCR_TEIE;

    tx_ch->CNDTR = (uint32_t)len;
    tx_ch->CPAR  = (uint32_t)&handle->instance->DR;
    tx_ch->CMAR  = (uint32_t)tx_data;

    /* Enable DMA requests in SPI (RX first) */
    REG_SET(handle->instance->CR2, SPI_CR2_RXDMAEN);
    REG_SET(handle->instance->CR2, SPI_CR2_TXDMAEN);

    /* Enable NVIC IRQs and start channels (RX before TX) */
    nvic_enable_irq(handle->dma_rx.irqn);
    nvic_enable_irq(handle->dma_tx.irqn);
    REG_SET(rx_ch->CCR, DMA_CCR_EN);
    REG_SET(tx_ch->CCR, DMA_CCR_EN);

    return 0;
}

/* ── ISR entry points ──────────────────────────────────────────────────── */

/**
 * @brief Handle error flags in the SPI status register
 *
 * Mode fault (MODF) is cleared by reading SR then writing CR1.
 * Overrun (OVR) is cleared by reading DR then SR.
 */
static void spi_handle_errors(spi_handle_t *handle, uint32_t sr)
{
    if (sr & SPI_SR_MODF) {
        handle->errors |= SPI_ERROR_MODF;
        /* Clear MODF: read SR (already done), then write CR1 */
        (void)handle->instance->CR1;
    }

    if (sr & SPI_SR_OVR) {
        handle->errors |= SPI_ERROR_OVR;
        /* Clear OVR: read DR then SR */
        (void)handle->instance->DR;
        (void)handle->instance->SR;
    }

    if (sr & SPI_SR_CRCERR) {
        handle->errors |= SPI_ERROR_CRC;
        /* Clear CRC error flag */
        REG_CLR(handle->instance->SR, SPI_SR_CRCERR);
    }

    if (handle->on_error) {
        handle->on_error(handle);
    }
}

void spi_irq_handler(spi_handle_t *handle)
{
    SPI_TypeDef *spi = handle->instance;
    uint32_t sr  = spi->SR;
    uint32_t cr2 = spi->CR2;

    /* ── Error detection ─────────────────────────────────────────────── */
    if ((cr2 & SPI_CR2_ERRIE) &&
        (sr & (SPI_SR_MODF | SPI_SR_OVR | SPI_SR_CRCERR))) {
        spi_handle_errors(handle, sr);
        sr = spi->SR;
    }

    /* ── RXNE: received data ready ───────────────────────────────────── */
    if ((sr & SPI_SR_RXNE) && (cr2 & SPI_CR2_RXNEIE)) {
        uint8_t byte = (uint8_t)(spi->DR & 0xFFU);
        ring_put(&handle->rx_ring, byte);

        if (handle->rx_remaining > 0) {
            handle->rx_remaining--;
        }

        if (handle->rx_remaining == 0 && handle->state == SPI_STATE_BUSY_RX) {
            /* All bytes received — disable RX interrupt */
            REG_CLR(spi->CR2, SPI_CR2_RXNEIE | SPI_CR2_ERRIE);
            handle->state = SPI_STATE_READY;

            if (handle->on_rx_complete) {
                handle->on_rx_complete(handle);
            }
        } else if (handle->state == SPI_STATE_BUSY_RX &&
                   handle->config.role == SPI_ROLE_MASTER) {
            /* Send another dummy to keep the clock running */
            spi->DR = SPI_DUMMY_BYTE;
        }
    }

    /* ── TXE: transmit buffer empty, feed next byte ──────────────────── */
    if ((sr & SPI_SR_TXE) && (cr2 & SPI_CR2_TXEIE)) {
        uint8_t byte;
        if (ring_get(&handle->tx_ring, &byte)) {
            spi->DR = byte;
            if (handle->tx_remaining > 0) {
                handle->tx_remaining--;
            }
        } else {
            /*
             * Ring buffer drained — disable TXE interrupt.
             * Wait for BSY to clear before signalling completion.
             */
            REG_CLR(spi->CR2, SPI_CR2_TXEIE);

            /* Drain any unread RX data to clear OVR */
            while (spi->SR & SPI_SR_RXNE) {
                (void)spi->DR;
            }

            /* Wait for last byte to finish shifting out */
            while (spi->SR & SPI_SR_BSY) {
                /* spin — typically 0-1 iteration at IRQ time */
            }

            handle->state = SPI_STATE_READY;

            if (handle->on_tx_complete) {
                handle->on_tx_complete(handle);
            }
        }
    }
}

void spi_dma_tx_handler(spi_handle_t *handle)
{
    uint8_t idx = handle->dma_tx.ch_index;
    uint32_t isr = DMA1->ISR;

    if (isr & DMA_ISR_TEIF(idx)) {
        /* DMA transfer error */
        DMA1->IFCR = DMA_ISR_TEIF(idx);
        handle->errors |= SPI_ERROR_DMA;
        if (handle->on_error) {
            handle->on_error(handle);
        }
    }

    if (isr & DMA_ISR_TCIF(idx)) {
        /* Transfer complete */
        DMA1->IFCR = DMA_ISR_TCIF(idx);

        /* Disable DMA channel and SPI DMA request */
        REG_CLR(handle->dma_tx.channel->CCR, DMA_CCR_EN);
        REG_CLR(handle->instance->CR2, SPI_CR2_TXDMAEN);

        handle->dma_tx.busy = false;

        /*
         * For TX-only DMA, signal completion once BSY clears.
         * For full-duplex, the RX handler signals completion.
         */
        if (handle->state == SPI_STATE_BUSY_TX) {
            while (handle->instance->SR & SPI_SR_BSY) {
                /* spin */
            }
            /* Clear OVR by reading DR then SR */
            (void)handle->instance->DR;
            (void)handle->instance->SR;

            handle->state = SPI_STATE_READY;

            if (handle->on_tx_complete) {
                handle->on_tx_complete(handle);
            }
        }
    }

    /* Clear any remaining flags */
    DMA1->IFCR = DMA_ISR_GIF(idx);
}

void spi_dma_rx_handler(spi_handle_t *handle)
{
    uint8_t idx = handle->dma_rx.ch_index;
    uint32_t isr = DMA1->ISR;

    if (isr & DMA_ISR_TEIF(idx)) {
        /* DMA transfer error */
        DMA1->IFCR = DMA_ISR_TEIF(idx);
        handle->errors |= SPI_ERROR_DMA;
        if (handle->on_error) {
            handle->on_error(handle);
        }
    }

    if (isr & DMA_ISR_TCIF(idx)) {
        /* Transfer complete */
        DMA1->IFCR = DMA_ISR_TCIF(idx);

        /* Disable DMA channel and SPI DMA request */
        REG_CLR(handle->dma_rx.channel->CCR, DMA_CCR_EN);
        REG_CLR(handle->instance->CR2, SPI_CR2_RXDMAEN);

        handle->dma_rx.busy = false;

        /* Also stop TX DMA if it was used for dummy bytes in RX-only mode */
        if (handle->dma_tx.busy) {
            REG_CLR(handle->dma_tx.channel->CCR, DMA_CCR_EN);
            REG_CLR(handle->instance->CR2, SPI_CR2_TXDMAEN);
            DMA1->IFCR = DMA_ISR_GIF(handle->dma_tx.ch_index);
            handle->dma_tx.busy = false;
        }

        if (handle->state == SPI_STATE_BUSY_RX) {
            handle->state = SPI_STATE_READY;
            if (handle->on_rx_complete) {
                handle->on_rx_complete(handle);
            }
        } else if (handle->state == SPI_STATE_BUSY_TX_RX) {
            handle->state = SPI_STATE_READY;
            if (handle->on_tx_rx_complete) {
                handle->on_tx_rx_complete(handle);
            }
        }
    }

    /* Clear any remaining flags */
    DMA1->IFCR = DMA_ISR_GIF(idx);
}
