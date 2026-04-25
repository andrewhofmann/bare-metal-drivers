/**
 * @file  dma.c
 * @brief DMA controller driver implementation for STM32F1xx
 *
 * Provides channel-level DMA management for the STM32F1 DMA1 controller.
 * Each channel is independently configured and managed through a
 * dma_handle_t that the caller allocates and passes to dma_init().
 *
 * The STM32F103 has a single DMA controller (DMA1) with 7 channels.
 * Each channel is hardwired to specific peripheral request lines
 * (see reference manual Table 78), but memory-to-memory transfers
 * can use any channel.
 */

#include "drivers/dma.h"

/* ── Internal helpers ──────────────────────────────────────────────────── */

/**
 * @brief NVIC IRQ numbers for DMA1 channels 1–7 (index 0–6)
 */
static const uint8_t dma_irqn_table[7] = {
    DMA1_Channel1_IRQn,
    DMA1_Channel2_IRQn,
    DMA1_Channel3_IRQn,
    DMA1_Channel4_IRQn,
    DMA1_Channel5_IRQn,
    DMA1_Channel6_IRQn,
    DMA1_Channel7_IRQn,
};

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
 * @brief Build the CCR register value from a dma_config_t
 *
 * Translates the driver-level configuration into the hardware bit-field
 * encoding expected by the DMA_CCR register.
 */
static uint32_t build_ccr(const dma_config_t *config)
{
    uint32_t ccr = 0;

    /* Transfer direction */
    if (config->direction == DMA_DIR_MEM_TO_PERIPH) {
        ccr |= DMA_CCR_DIR;
    } else if (config->direction == DMA_DIR_MEM_TO_MEM) {
        ccr |= DMA_CCR_MEM2MEM;
        /*
         * For memory-to-memory, DIR controls which address auto-increments
         * as "source". We read from CPAR (periph_addr) and write to CMAR
         * (mem_addr), so DIR=0 (periph→mem) is correct.
         */
    }
    /* DMA_DIR_PERIPH_TO_MEM: DIR=0 (default) */

    /* Peripheral data size */
    ccr |= ((uint32_t)config->periph_data_size << 8);

    /* Memory data size */
    ccr |= ((uint32_t)config->mem_data_size << 10);

    /* Address increment modes */
    if (config->periph_inc) {
        ccr |= DMA_CCR_PINC;
    }
    if (config->mem_inc) {
        ccr |= DMA_CCR_MINC;
    }

    /* Circular mode (not valid for mem-to-mem) */
    if (config->circular && config->direction != DMA_DIR_MEM_TO_MEM) {
        ccr |= DMA_CCR_CIRC;
    }

    /* Priority level */
    ccr |= ((uint32_t)config->priority << 12);

    /* Interrupt enables */
    if (config->enable_tc_irq) {
        ccr |= DMA_CCR_TCIE;
    }
    if (config->enable_ht_irq) {
        ccr |= DMA_CCR_HTIE;
    }
    if (config->enable_te_irq) {
        ccr |= DMA_CCR_TEIE;
    }

    return ccr;
}

/* ── Initialisation / deinitialisation ─────────────────────────────────── */

void dma_init(dma_handle_t *handle, dma_channel_t channel,
              const dma_config_t *config)
{
    handle->ch_index = channel;
    handle->channel  = &DMA1->CH[channel];
    handle->irqn     = dma_irqn_table[channel];
    handle->config   = *config;
    handle->state    = DMA_STATE_RESET;
    handle->errors   = DMA_ERROR_NONE;

    handle->on_transfer_complete = (void *)0;
    handle->on_half_transfer     = (void *)0;
    handle->on_error             = (void *)0;

    /* Enable DMA1 peripheral clock */
    REG_SET(RCC->AHBENR, RCC_AHBENR_DMA1EN);

    /* Make sure the channel is disabled */
    REG_CLR(handle->channel->CCR, DMA_CCR_EN);

    /* Clear any pending interrupt flags for this channel */
    DMA1->IFCR = DMA_ISR_GIF(channel);

    handle->state = DMA_STATE_READY;
}

void dma_deinit(dma_handle_t *handle)
{
    /* Disable the channel */
    REG_CLR(handle->channel->CCR, DMA_CCR_EN);

    /* Clear all interrupt flags for this channel */
    DMA1->IFCR = DMA_ISR_GIF(handle->ch_index);

    /* Disable NVIC IRQ */
    nvic_disable_irq(handle->irqn);

    /* Reset the channel registers */
    handle->channel->CCR   = 0;
    handle->channel->CNDTR = 0;
    handle->channel->CPAR  = 0;
    handle->channel->CMAR  = 0;

    handle->state  = DMA_STATE_RESET;
    handle->errors = DMA_ERROR_NONE;
}

/* ── Transfer control ──────────────────────────────────────────────────── */

int dma_start(dma_handle_t *handle, uint32_t periph_addr,
              uint32_t mem_addr, uint16_t length)
{
    if (handle->state == DMA_STATE_RESET || handle->state == DMA_STATE_BUSY) {
        return -1;
    }

    DMA_Channel_TypeDef *ch = handle->channel;

    /* Disable channel before reconfiguration */
    REG_CLR(ch->CCR, DMA_CCR_EN);

    /* Clear any pending interrupt flags */
    DMA1->IFCR = DMA_ISR_GIF(handle->ch_index);

    /* Program the channel registers */
    ch->CCR   = build_ccr(&handle->config);
    ch->CNDTR = (uint32_t)length;
    ch->CPAR  = periph_addr;
    ch->CMAR  = mem_addr;

    handle->errors = DMA_ERROR_NONE;
    handle->state  = DMA_STATE_BUSY;

    /* Enable NVIC IRQ if any interrupt is configured */
    if (handle->config.enable_tc_irq || handle->config.enable_ht_irq ||
        handle->config.enable_te_irq) {
        nvic_enable_irq(handle->irqn);
    }

    /* Start the transfer */
    REG_SET(ch->CCR, DMA_CCR_EN);

    return 0;
}

void dma_stop(dma_handle_t *handle)
{
    /* Disable the channel */
    REG_CLR(handle->channel->CCR, DMA_CCR_EN);

    /* Clear interrupt flags */
    DMA1->IFCR = DMA_ISR_GIF(handle->ch_index);

    handle->state = DMA_STATE_READY;
}

void dma_abort(dma_handle_t *handle)
{
    /* Disable the channel */
    REG_CLR(handle->channel->CCR, DMA_CCR_EN);

    /* Clear interrupt flags */
    DMA1->IFCR = DMA_ISR_GIF(handle->ch_index);

    handle->state = DMA_STATE_ERROR;
}

/* ── Status queries ────────────────────────────────────────────────────── */

dma_state_t dma_get_state(const dma_handle_t *handle)
{
    return handle->state;
}

dma_error_t dma_get_error(const dma_handle_t *handle)
{
    return handle->errors;
}

uint16_t dma_get_remaining(const dma_handle_t *handle)
{
    return (uint16_t)(handle->channel->CNDTR & 0xFFFFU);
}

bool dma_poll(dma_handle_t *handle)
{
    if (handle->state != DMA_STATE_BUSY &&
        handle->state != DMA_STATE_HALF) {
        return true;
    }

    uint32_t isr = DMA1->ISR;
    uint8_t idx = handle->ch_index;

    /* Check for transfer error first */
    if (isr & DMA_ISR_TEIF(idx)) {
        DMA1->IFCR = DMA_ISR_TEIF(idx);
        REG_CLR(handle->channel->CCR, DMA_CCR_EN);
        handle->errors |= DMA_ERROR_TRANSFER;
        handle->state = DMA_STATE_ERROR;
        return true;
    }

    /* Check for transfer complete */
    if (isr & DMA_ISR_TCIF(idx)) {
        DMA1->IFCR = DMA_ISR_TCIF(idx);

        /* In non-circular mode, the channel auto-disables after TC */
        if (!handle->config.circular) {
            handle->state = DMA_STATE_COMPLETE;
        } else {
            /* Circular mode: stays busy, counter reloads automatically */
            handle->state = DMA_STATE_BUSY;
        }
        return true;
    }

    /* Check for half-transfer */
    if (isr & DMA_ISR_HTIF(idx)) {
        DMA1->IFCR = DMA_ISR_HTIF(idx);
        handle->state = DMA_STATE_HALF;
    }

    return false;
}

/* ── ISR entry point ──────────────────────────────────────────────────── */

void dma_irq_handler(dma_handle_t *handle)
{
    uint32_t isr = DMA1->ISR;
    uint8_t idx = handle->ch_index;

    /* ── Transfer error ─────────────────────────────────────────────── */
    if (isr & DMA_ISR_TEIF(idx)) {
        DMA1->IFCR = DMA_ISR_TEIF(idx);

        /* Disable the channel on error */
        REG_CLR(handle->channel->CCR, DMA_CCR_EN);

        handle->errors |= DMA_ERROR_TRANSFER;
        handle->state = DMA_STATE_ERROR;

        if (handle->on_error) {
            handle->on_error(handle);
        }
    }

    /* ── Half-transfer ──────────────────────────────────────────────── */
    if (isr & DMA_ISR_HTIF(idx)) {
        DMA1->IFCR = DMA_ISR_HTIF(idx);

        handle->state = DMA_STATE_HALF;

        if (handle->on_half_transfer) {
            handle->on_half_transfer(handle);
        }
    }

    /* ── Transfer complete ──────────────────────────────────────────── */
    if (isr & DMA_ISR_TCIF(idx)) {
        DMA1->IFCR = DMA_ISR_TCIF(idx);

        if (!handle->config.circular) {
            handle->state = DMA_STATE_COMPLETE;
        } else {
            /* Circular mode: channel keeps running, counter reloads */
            handle->state = DMA_STATE_BUSY;
        }

        if (handle->on_transfer_complete) {
            handle->on_transfer_complete(handle);
        }
    }

    /* Clear the global interrupt flag */
    DMA1->IFCR = DMA_ISR_GIF(idx);
}
