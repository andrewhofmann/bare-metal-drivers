/**
 * @file  uart.h
 * @brief UART driver interface for STM32F1xx
 *
 * Supports three transfer modes:
 *   - Polling:    blocking send/receive
 *   - Interrupt:  non-blocking with internal ring buffers
 *   - DMA:        non-blocking bulk transfers via DMA channels
 *
 * Each USART instance is managed through a uart_handle_t that tracks
 * peripheral state, buffers, and user callbacks.
 */

#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f1xx.h"

/* ── Configuration constants ───────────────────────────────────────────── */

/** Size of the internal ring buffers used for interrupt-driven transfers */
#ifndef UART_RING_BUF_SIZE
#define UART_RING_BUF_SIZE  128
#endif

/* ── Enumerations ──────────────────────────────────────────────────────── */

/**
 * @brief Word length (data bits)
 *
 * When parity is enabled the parity bit is included in the word length,
 * so UART_WORD_9BIT is required to get 8 data bits + parity.
 */
typedef enum {
    UART_WORD_8BIT = 0,
    UART_WORD_9BIT = 1,
} uart_word_len_t;

/**
 * @brief Stop-bit configuration
 */
typedef enum {
    UART_STOP_1   = 0,     /**< 1 stop bit   */
    UART_STOP_0_5 = 1,     /**< 0.5 stop bits */
    UART_STOP_2   = 2,     /**< 2 stop bits  */
    UART_STOP_1_5 = 3,     /**< 1.5 stop bits */
} uart_stop_bits_t;

/**
 * @brief Parity mode
 */
typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN = 1,
    UART_PARITY_ODD  = 2,
} uart_parity_t;

/**
 * @brief Direction (TX, RX, or both)
 */
typedef enum {
    UART_DIR_TX    = 0x01,
    UART_DIR_RX    = 0x02,
    UART_DIR_TX_RX = 0x03,
} uart_dir_t;

/**
 * @brief Current transfer state of a UART handle
 */
typedef enum {
    UART_STATE_RESET = 0,   /**< Not initialised              */
    UART_STATE_READY,       /**< Idle, ready for transfer      */
    UART_STATE_BUSY_TX,     /**< Non-blocking TX in progress   */
    UART_STATE_BUSY_RX,     /**< Non-blocking RX in progress   */
    UART_STATE_BUSY_TX_RX,  /**< Both directions active        */
} uart_state_t;

/**
 * @brief Error flags (can be ORed together)
 */
typedef enum {
    UART_ERROR_NONE     = 0x00,
    UART_ERROR_PARITY   = 0x01,
    UART_ERROR_FRAMING  = 0x02,
    UART_ERROR_NOISE    = 0x04,
    UART_ERROR_OVERRUN  = 0x08,
    UART_ERROR_DMA      = 0x10,
} uart_error_t;

/* ── Structures ────────────────────────────────────────────────────────── */

/**
 * @brief Static UART configuration applied during uart_init()
 */
typedef struct {
    uint32_t         baud_rate;     /**< Desired baud rate in bps        */
    uart_word_len_t  word_length;   /**< 8-bit or 9-bit word             */
    uart_stop_bits_t stop_bits;     /**< Stop-bit count                  */
    uart_parity_t    parity;        /**< Parity mode                     */
    uart_dir_t       direction;     /**< TX, RX, or both                 */
} uart_config_t;

/**
 * @brief Simple ring buffer used by interrupt-driven transfers
 */
typedef struct {
    uint8_t  buf[UART_RING_BUF_SIZE];
    volatile uint16_t head;         /**< Next write index                */
    volatile uint16_t tail;         /**< Next read index                 */
} uart_ring_buf_t;

/**
 * @brief DMA transfer state for one direction
 */
typedef struct {
    DMA_Channel_TypeDef *channel;   /**< DMA channel register block      */
    uint8_t              ch_index;  /**< Channel index 0–6 (for ISR/IFCR)*/
    uint8_t              irqn;      /**< NVIC IRQ number                 */
    volatile bool        busy;      /**< Transfer in progress            */
} uart_dma_t;

/** Forward declaration for callback signatures */
typedef struct uart_handle uart_handle_t;

/**
 * @brief User callback type for transfer-complete and error events
 */
typedef void (*uart_callback_t)(uart_handle_t *handle);

/**
 * @brief UART handle — one per USART instance
 *
 * Allocate statically and pass to uart_init(). Do not modify fields
 * directly after initialisation — use the driver API.
 */
struct uart_handle {
    USART_TypeDef  *instance;       /**< USART peripheral pointer        */
    uart_config_t   config;         /**< Active configuration            */
    volatile uart_state_t state;    /**< Transfer state machine          */
    volatile uart_error_t errors;   /**< Accumulated error flags         */

    /* Interrupt-mode ring buffers */
    uart_ring_buf_t tx_ring;
    uart_ring_buf_t rx_ring;

    /* DMA descriptors (NULL channel pointer = DMA not configured) */
    uart_dma_t      dma_tx;
    uart_dma_t      dma_rx;

    /* User callbacks — NULL means no notification */
    uart_callback_t on_tx_complete; /**< All TX data sent                */
    uart_callback_t on_rx_complete; /**< RX DMA transfer finished        */
    uart_callback_t on_error;       /**< Error detected during transfer  */
};

/* ── Initialisation / deinitialisation ─────────────────────────────────── */

/**
 * @brief Initialise a USART peripheral
 *
 * Enables the peripheral clock, configures baud rate, word length,
 * stop bits, parity, and direction, then enables the USART.
 * The caller must enable the GPIO clocks and configure the TX/RX pins
 * to alternate-function mode before calling this.
 *
 * @param handle  Pointer to a caller-allocated handle
 * @param usart   USART peripheral (USART1, USART2, or USART3)
 * @param config  Desired configuration
 */
void uart_init(uart_handle_t *handle, USART_TypeDef *usart,
               const uart_config_t *config);

/**
 * @brief Disable the USART peripheral and reset the handle
 */
void uart_deinit(uart_handle_t *handle);

/* ── Polling (blocking) transfers ──────────────────────────────────────── */

/**
 * @brief Transmit a buffer of bytes (blocking)
 *
 * Waits for TXE before writing each byte and waits for TC after the
 * last byte to ensure the shift register is empty on return.
 *
 * @param handle  Initialised UART handle
 * @param data    Data to transmit
 * @param len     Number of bytes
 */
void uart_transmit(uart_handle_t *handle, const uint8_t *data, size_t len);

/**
 * @brief Receive a fixed number of bytes (blocking)
 *
 * Waits for RXNE before reading each byte.
 *
 * @param handle  Initialised UART handle
 * @param data    Buffer to store received bytes
 * @param len     Number of bytes to receive
 */
void uart_receive(uart_handle_t *handle, uint8_t *data, size_t len);

/* ── Interrupt-driven transfers ────────────────────────────────────────── */

/**
 * @brief Start an interrupt-driven transmission
 *
 * Copies data into the TX ring buffer and enables the TXE interrupt.
 * Returns immediately — the ISR drains the ring buffer in the
 * background. Calls on_tx_complete when the ring buffer empties.
 *
 * @param handle  Initialised UART handle
 * @param data    Data to queue for transmission
 * @param len     Number of bytes (silently truncated to available space)
 * @return Number of bytes actually queued
 */
size_t uart_transmit_it(uart_handle_t *handle, const uint8_t *data,
                        size_t len);

/**
 * @brief Enable interrupt-driven reception
 *
 * Enables the RXNE interrupt. Incoming bytes are stored in the RX
 * ring buffer and can be retrieved with uart_read_rx_buffer().
 *
 * @param handle  Initialised UART handle
 */
void uart_receive_it(uart_handle_t *handle);

/**
 * @brief Stop interrupt-driven reception
 */
void uart_stop_receive_it(uart_handle_t *handle);

/**
 * @brief Read available bytes from the RX ring buffer
 *
 * @param handle   Initialised UART handle
 * @param data     Destination buffer
 * @param max_len  Maximum number of bytes to read
 * @return Number of bytes actually copied
 */
size_t uart_read_rx_buffer(uart_handle_t *handle, uint8_t *data,
                           size_t max_len);

/**
 * @brief Return the number of unread bytes in the RX ring buffer
 */
size_t uart_rx_available(const uart_handle_t *handle);

/* ── DMA transfers ─────────────────────────────────────────────────────── */

/**
 * @brief Configure the DMA channels used by this UART
 *
 * Must be called after uart_init() and before any DMA transfer.
 * Enables the DMA1 clock and stores the channel assignments.
 *
 * @param handle     Initialised UART handle
 * @param tx_channel DMA channel for TX (e.g. &DMA1->CH[3] for ch4)
 * @param tx_index   Channel index 0–6 (for ISR/IFCR flag offsets)
 * @param tx_irqn    NVIC IRQ number for the TX DMA channel
 * @param rx_channel DMA channel for RX
 * @param rx_index   Channel index 0–6
 * @param rx_irqn    NVIC IRQ number for the RX DMA channel
 */
void uart_config_dma(uart_handle_t *handle,
                     DMA_Channel_TypeDef *tx_channel, uint8_t tx_index,
                     uint8_t tx_irqn,
                     DMA_Channel_TypeDef *rx_channel, uint8_t rx_index,
                     uint8_t rx_irqn);

/**
 * @brief Transmit a buffer via DMA (non-blocking)
 *
 * The caller must keep the data buffer valid until on_tx_complete fires
 * or uart_dma_tx_handler() reports completion.
 *
 * @param handle  UART handle with DMA configured
 * @param data    Source buffer (must remain valid until transfer completes)
 * @param len     Number of bytes to transmit
 * @return 0 on success, -1 if DMA is not configured or already busy
 */
int uart_transmit_dma(uart_handle_t *handle, const uint8_t *data,
                      size_t len);

/**
 * @brief Receive a fixed number of bytes via DMA (non-blocking)
 *
 * @param handle  UART handle with DMA configured
 * @param data    Destination buffer (must remain valid until transfer ends)
 * @param len     Number of bytes to receive
 * @return 0 on success, -1 if DMA is not configured or already busy
 */
int uart_receive_dma(uart_handle_t *handle, uint8_t *data, size_t len);

/* ── ISR entry points (call from vector-table handlers) ────────────────── */

/**
 * @brief USART interrupt handler
 *
 * Call this from the USARTx_IRQHandler() for the corresponding instance.
 * Handles TXE, RXNE, TC, and error flags.
 */
void uart_irq_handler(uart_handle_t *handle);

/**
 * @brief DMA TX transfer-complete handler
 *
 * Call this from the DMA channel IRQ handler assigned to TX.
 */
void uart_dma_tx_handler(uart_handle_t *handle);

/**
 * @brief DMA RX transfer-complete handler
 *
 * Call this from the DMA channel IRQ handler assigned to RX.
 */
void uart_dma_rx_handler(uart_handle_t *handle);

#endif /* DRIVERS_UART_H */
