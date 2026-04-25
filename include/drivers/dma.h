/**
 * @file  dma.h
 * @brief DMA controller driver interface for STM32F1xx
 *
 * Provides a channel-based API for configuring and running DMA transfers
 * on the STM32F1 DMA1 controller (7 channels). Supports:
 *   - Peripheral-to-memory transfers
 *   - Memory-to-peripheral transfers
 *   - Memory-to-memory transfers
 *   - Circular mode for continuous streaming
 *   - Half-transfer and transfer-complete callbacks
 *
 * Each DMA channel is managed through a dma_handle_t that tracks the
 * channel configuration, transfer state, and user callbacks.
 *
 * DMA1 channel request mapping (STM32F103, reference manual Table 78):
 *   Ch1: ADC1 / TIM2_CH3 / TIM4_CH1
 *   Ch2: SPI1_RX / USART3_TX / TIM1_CH1 / TIM2_UP / TIM3_CH3
 *   Ch3: SPI1_TX / USART3_RX / TIM1_CH2 / TIM3_CH4 / TIM3_UP
 *   Ch4: SPI2_RX / USART1_TX / I2C2_TX / TIM1_CH4 / TIM1_TRIG / TIM1_COM / TIM4_CH2
 *   Ch5: SPI2_TX / USART1_RX / I2C2_RX / TIM1_UP / TIM2_CH1 / TIM4_CH3
 *   Ch6: USART2_RX / I2C1_TX / TIM1_CH3 / TIM3_CH1 / TIM3_TRIG
 *   Ch7: USART2_TX / I2C1_RX / TIM2_CH2 / TIM2_CH4 / TIM4_UP
 */

#ifndef DRIVERS_DMA_H
#define DRIVERS_DMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f1xx.h"

/* ── Enumerations ──────────────────────────────────────────────────────── */

/**
 * @brief DMA channel index (0-based, maps to DMA1 channels 1–7)
 */
typedef enum {
    DMA_CHANNEL_1 = 0,
    DMA_CHANNEL_2 = 1,
    DMA_CHANNEL_3 = 2,
    DMA_CHANNEL_4 = 3,
    DMA_CHANNEL_5 = 4,
    DMA_CHANNEL_6 = 5,
    DMA_CHANNEL_7 = 6,
} dma_channel_t;

/**
 * @brief Transfer direction
 */
typedef enum {
    DMA_DIR_PERIPH_TO_MEM = 0,  /**< Peripheral → memory (DIR=0) */
    DMA_DIR_MEM_TO_PERIPH = 1,  /**< Memory → peripheral (DIR=1) */
    DMA_DIR_MEM_TO_MEM    = 2,  /**< Memory → memory (MEM2MEM=1) */
} dma_dir_t;

/**
 * @brief Data width for source or destination
 */
typedef enum {
    DMA_DATA_SIZE_8  = 0,   /**< Byte (8-bit)       */
    DMA_DATA_SIZE_16 = 1,   /**< Half-word (16-bit)  */
    DMA_DATA_SIZE_32 = 2,   /**< Word (32-bit)       */
} dma_data_size_t;

/**
 * @brief Channel priority level
 */
typedef enum {
    DMA_PRIORITY_LOW       = 0,
    DMA_PRIORITY_MEDIUM    = 1,
    DMA_PRIORITY_HIGH      = 2,
    DMA_PRIORITY_VERY_HIGH = 3,
} dma_priority_t;

/**
 * @brief Current state of a DMA channel handle
 */
typedef enum {
    DMA_STATE_RESET = 0,    /**< Not initialised             */
    DMA_STATE_READY,        /**< Configured, idle            */
    DMA_STATE_BUSY,         /**< Transfer in progress        */
    DMA_STATE_HALF,         /**< Half-transfer reached       */
    DMA_STATE_COMPLETE,     /**< Transfer finished           */
    DMA_STATE_ERROR,        /**< Transfer error occurred     */
} dma_state_t;

/**
 * @brief Error flags (can be ORed together)
 */
typedef enum {
    DMA_ERROR_NONE     = 0x00,
    DMA_ERROR_TRANSFER = 0x01,  /**< Transfer error (TEIF)   */
    DMA_ERROR_CONFIG   = 0x02,  /**< Invalid configuration   */
} dma_error_t;

/* ── Structures ────────────────────────────────────────────────────────── */

/**
 * @brief DMA channel configuration
 *
 * Populate this structure and pass it to dma_init() to configure a
 * DMA channel. For memory-to-memory transfers set direction to
 * DMA_DIR_MEM_TO_MEM and use periph_addr as the source memory address.
 */
typedef struct {
    dma_dir_t        direction;      /**< Transfer direction              */
    dma_data_size_t  periph_data_size; /**< Peripheral (or src) data width */
    dma_data_size_t  mem_data_size;   /**< Memory (or dst) data width     */
    bool             periph_inc;     /**< Increment peripheral address    */
    bool             mem_inc;        /**< Increment memory address        */
    bool             circular;       /**< Circular mode (auto-reload)     */
    dma_priority_t   priority;       /**< Channel priority                */
    bool             enable_tc_irq;  /**< Enable transfer-complete IRQ    */
    bool             enable_ht_irq;  /**< Enable half-transfer IRQ        */
    bool             enable_te_irq;  /**< Enable transfer-error IRQ       */
} dma_config_t;

/** Forward declaration for callback signatures */
typedef struct dma_handle dma_handle_t;

/**
 * @brief User callback type for DMA events
 */
typedef void (*dma_callback_t)(dma_handle_t *handle);

/**
 * @brief DMA channel handle — one per active channel
 *
 * Allocate statically and pass to dma_init(). Do not modify fields
 * directly after initialisation — use the driver API.
 */
struct dma_handle {
    DMA_Channel_TypeDef *channel;    /**< Channel register block          */
    dma_channel_t        ch_index;   /**< Channel index 0–6               */
    uint8_t              irqn;       /**< NVIC IRQ number for this channel*/
    dma_config_t         config;     /**< Active configuration            */
    volatile dma_state_t state;      /**< Transfer state                  */
    volatile dma_error_t errors;     /**< Accumulated error flags         */

    /* User callbacks — NULL means no notification */
    dma_callback_t on_transfer_complete; /**< Full transfer done          */
    dma_callback_t on_half_transfer;     /**< Half of transfer done       */
    dma_callback_t on_error;             /**< Transfer error              */
};

/* ── Initialisation / deinitialisation ─────────────────────────────────── */

/**
 * @brief Initialise a DMA channel
 *
 * Enables the DMA1 peripheral clock (if not already enabled), stores
 * the configuration, and prepares the channel for transfers. Does not
 * start a transfer — call dma_start() for that.
 *
 * @param handle   Pointer to a caller-allocated handle
 * @param channel  Channel index (DMA_CHANNEL_1 .. DMA_CHANNEL_7)
 * @param config   Desired channel configuration
 */
void dma_init(dma_handle_t *handle, dma_channel_t channel,
              const dma_config_t *config);

/**
 * @brief Disable a DMA channel and reset the handle
 *
 * Aborts any active transfer, disables the channel interrupt in the
 * NVIC, and clears all pending flags.
 */
void dma_deinit(dma_handle_t *handle);

/* ── Transfer control ──────────────────────────────────────────────────── */

/**
 * @brief Start a DMA transfer
 *
 * Configures the channel registers (CCR, CPAR, CMAR, CNDTR) from the
 * stored configuration and the provided addresses/length, then enables
 * the channel. For circular-mode transfers, the channel runs
 * continuously until stopped with dma_stop().
 *
 * @param handle      Initialised DMA handle
 * @param periph_addr Peripheral register address (or source for mem-to-mem)
 * @param mem_addr    Memory buffer address (or destination for mem-to-mem)
 * @param length      Number of data items to transfer
 * @return 0 on success, -1 if the channel is busy or not initialised
 */
int dma_start(dma_handle_t *handle, uint32_t periph_addr,
              uint32_t mem_addr, uint16_t length);

/**
 * @brief Stop an active DMA transfer
 *
 * Disables the channel and clears interrupt flags. The handle returns
 * to the READY state.
 */
void dma_stop(dma_handle_t *handle);

/**
 * @brief Abort an active DMA transfer and report an error
 *
 * Like dma_stop(), but sets the handle state to ERROR. Useful for
 * timeout or watchdog-triggered aborts.
 */
void dma_abort(dma_handle_t *handle);

/* ── Status queries ────────────────────────────────────────────────────── */

/**
 * @brief Return the current state of a DMA channel
 */
dma_state_t dma_get_state(const dma_handle_t *handle);

/**
 * @brief Return the accumulated error flags
 */
dma_error_t dma_get_error(const dma_handle_t *handle);

/**
 * @brief Return the number of remaining data items in the current transfer
 *
 * Reads the CNDTR register. Returns 0 when no transfer is active.
 */
uint16_t dma_get_remaining(const dma_handle_t *handle);

/**
 * @brief Poll for transfer complete (non-interrupt usage)
 *
 * Checks the DMA ISR flags and updates the handle state accordingly.
 * Call this in a polling loop when interrupts are not enabled.
 *
 * @param handle  Initialised DMA handle with an active transfer
 * @return true if the transfer has completed (or errored), false if still busy
 */
bool dma_poll(dma_handle_t *handle);

/* ── ISR entry point ──────────────────────────────────────────────────── */

/**
 * @brief DMA channel interrupt handler
 *
 * Call this from the DMA1_ChannelX_IRQHandler() for the corresponding
 * channel. Handles transfer-complete, half-transfer, and error flags.
 */
void dma_irq_handler(dma_handle_t *handle);

#endif /* DRIVERS_DMA_H */
