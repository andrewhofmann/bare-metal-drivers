/**
 * @file  spi.h
 * @brief SPI driver interface for STM32F1xx
 *
 * Supports three transfer modes:
 *   - Polling:    blocking send/receive
 *   - Interrupt:  non-blocking with internal ring buffers
 *   - DMA:        non-blocking bulk transfers via DMA channels
 *
 * All four SPI clock modes (CPOL/CPHA combinations) are configurable.
 * Each SPI instance is managed through an spi_handle_t that tracks
 * peripheral state, buffers, and user callbacks.
 */

#ifndef DRIVERS_SPI_H
#define DRIVERS_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f1xx.h"

/* ── Configuration constants ───────────────────────────────────────────── */

/** Size of the internal ring buffers used for interrupt-driven transfers */
#ifndef SPI_RING_BUF_SIZE
#define SPI_RING_BUF_SIZE  128
#endif

/* ── Enumerations ──────────────────────────────────────────────────────── */

/**
 * @brief SPI clock mode (CPOL | CPHA)
 *
 * Mode 0: CPOL=0, CPHA=0  — idle low,  capture on first (rising) edge
 * Mode 1: CPOL=0, CPHA=1  — idle low,  capture on second (falling) edge
 * Mode 2: CPOL=1, CPHA=0  — idle high, capture on first (falling) edge
 * Mode 3: CPOL=1, CPHA=1  — idle high, capture on second (rising) edge
 */
typedef enum {
    SPI_MODE_0 = 0,                             /**< CPOL=0, CPHA=0 */
    SPI_MODE_1 = SPI_CR1_CPHA,                  /**< CPOL=0, CPHA=1 */
    SPI_MODE_2 = SPI_CR1_CPOL,                  /**< CPOL=1, CPHA=0 */
    SPI_MODE_3 = SPI_CR1_CPOL | SPI_CR1_CPHA,   /**< CPOL=1, CPHA=1 */
} spi_mode_t;

/**
 * @brief Master or slave role
 */
typedef enum {
    SPI_ROLE_SLAVE  = 0,
    SPI_ROLE_MASTER = 1,
} spi_role_t;

/**
 * @brief Data frame format
 */
typedef enum {
    SPI_DATA_8BIT  = 0,     /**< 8-bit data frame  */
    SPI_DATA_16BIT = 1,     /**< 16-bit data frame */
} spi_data_size_t;

/**
 * @brief Bit order
 */
typedef enum {
    SPI_MSB_FIRST = 0,
    SPI_LSB_FIRST = 1,
} spi_bit_order_t;

/**
 * @brief Baud rate prescaler
 *
 * The SPI clock is derived from the peripheral bus clock (APB2 for SPI1,
 * APB1 for SPI2) divided by the selected prescaler.
 */
typedef enum {
    SPI_PRESCALER_2   = 0,  /**< f_pclk / 2   */
    SPI_PRESCALER_4   = 1,  /**< f_pclk / 4   */
    SPI_PRESCALER_8   = 2,  /**< f_pclk / 8   */
    SPI_PRESCALER_16  = 3,  /**< f_pclk / 16  */
    SPI_PRESCALER_32  = 4,  /**< f_pclk / 32  */
    SPI_PRESCALER_64  = 5,  /**< f_pclk / 64  */
    SPI_PRESCALER_128 = 6,  /**< f_pclk / 128 */
    SPI_PRESCALER_256 = 7,  /**< f_pclk / 256 */
} spi_prescaler_t;

/**
 * @brief NSS (chip-select) management mode
 */
typedef enum {
    SPI_NSS_SOFT = 0,       /**< Software NSS management (SSM=1, SSI=1) */
    SPI_NSS_HARD = 1,       /**< Hardware NSS management (SSM=0, SSOE)  */
} spi_nss_t;

/**
 * @brief Current transfer state of an SPI handle
 */
typedef enum {
    SPI_STATE_RESET = 0,    /**< Not initialised              */
    SPI_STATE_READY,        /**< Idle, ready for transfer      */
    SPI_STATE_BUSY_TX,      /**< Non-blocking TX in progress   */
    SPI_STATE_BUSY_RX,      /**< Non-blocking RX in progress   */
    SPI_STATE_BUSY_TX_RX,   /**< Full-duplex transfer active   */
} spi_state_t;

/**
 * @brief Error flags (can be ORed together)
 */
typedef enum {
    SPI_ERROR_NONE    = 0x00,
    SPI_ERROR_MODF    = 0x01,   /**< Mode fault                */
    SPI_ERROR_OVR     = 0x02,   /**< Overrun error             */
    SPI_ERROR_CRC     = 0x04,   /**< CRC mismatch              */
    SPI_ERROR_DMA     = 0x08,   /**< DMA transfer error        */
} spi_error_t;

/* ── Structures ────────────────────────────────────────────────────────── */

/**
 * @brief Static SPI configuration applied during spi_init()
 */
typedef struct {
    spi_mode_t       mode;          /**< Clock mode (0–3)                */
    spi_role_t       role;          /**< Master or slave                 */
    spi_data_size_t  data_size;     /**< 8-bit or 16-bit frame           */
    spi_bit_order_t  bit_order;     /**< MSB-first or LSB-first          */
    spi_prescaler_t  prescaler;     /**< Baud rate prescaler             */
    spi_nss_t        nss;           /**< NSS management mode             */
} spi_config_t;

/**
 * @brief Simple ring buffer used by interrupt-driven transfers
 */
typedef struct {
    uint8_t  buf[SPI_RING_BUF_SIZE];
    volatile uint16_t head;         /**< Next write index                */
    volatile uint16_t tail;         /**< Next read index                 */
} spi_ring_buf_t;

/**
 * @brief DMA transfer state for one direction
 */
typedef struct {
    DMA_Channel_TypeDef *channel;   /**< DMA channel register block      */
    uint8_t              ch_index;  /**< Channel index 0–6 (for ISR/IFCR)*/
    uint8_t              irqn;      /**< NVIC IRQ number                 */
    volatile bool        busy;      /**< Transfer in progress            */
} spi_dma_t;

/** Forward declaration for callback signatures */
typedef struct spi_handle spi_handle_t;

/**
 * @brief User callback type for transfer-complete and error events
 */
typedef void (*spi_callback_t)(spi_handle_t *handle);

/**
 * @brief SPI handle — one per SPI instance
 *
 * Allocate statically and pass to spi_init(). Do not modify fields
 * directly after initialisation — use the driver API.
 */
struct spi_handle {
    SPI_TypeDef    *instance;       /**< SPI peripheral pointer          */
    spi_config_t    config;         /**< Active configuration            */
    volatile spi_state_t state;     /**< Transfer state machine          */
    volatile spi_error_t errors;    /**< Accumulated error flags         */

    /* Interrupt-mode ring buffers */
    spi_ring_buf_t  tx_ring;
    spi_ring_buf_t  rx_ring;

    /* Interrupt-mode transfer tracking */
    volatile uint16_t tx_remaining; /**< Bytes left to transmit          */
    volatile uint16_t rx_remaining; /**< Bytes left to receive           */

    /* DMA descriptors (NULL channel pointer = DMA not configured) */
    spi_dma_t       dma_tx;
    spi_dma_t       dma_rx;

    /* User callbacks — NULL means no notification */
    spi_callback_t  on_tx_complete; /**< All TX data sent                */
    spi_callback_t  on_rx_complete; /**< RX transfer finished            */
    spi_callback_t  on_tx_rx_complete; /**< Full-duplex transfer done    */
    spi_callback_t  on_error;       /**< Error detected during transfer  */
};

/* ── Initialisation / deinitialisation ─────────────────────────────────── */

/**
 * @brief Initialise an SPI peripheral
 *
 * Enables the peripheral clock, configures clock mode, data size,
 * prescaler, bit order, role, and NSS management, then enables the SPI.
 * The caller must enable the GPIO clocks and configure the SCK, MOSI,
 * MISO, and (optionally) NSS pins before calling this.
 *
 * @param handle  Pointer to a caller-allocated handle
 * @param spi     SPI peripheral (SPI1 or SPI2)
 * @param config  Desired configuration
 */
void spi_init(spi_handle_t *handle, SPI_TypeDef *spi,
              const spi_config_t *config);

/**
 * @brief Disable the SPI peripheral and reset the handle
 */
void spi_deinit(spi_handle_t *handle);

/* ── Polling (blocking) transfers ──────────────────────────────────────── */

/**
 * @brief Transmit a buffer of bytes (blocking)
 *
 * Sends data while discarding received bytes. Waits for TXE before
 * writing each byte and waits for BSY to clear after the last byte.
 *
 * @param handle  Initialised SPI handle
 * @param data    Data to transmit
 * @param len     Number of bytes (or 16-bit words if DFF=16)
 */
void spi_transmit(spi_handle_t *handle, const uint8_t *data, size_t len);

/**
 * @brief Receive a fixed number of bytes (blocking)
 *
 * Sends dummy bytes (0xFF) to generate clock and reads the response.
 *
 * @param handle  Initialised SPI handle
 * @param data    Buffer to store received bytes
 * @param len     Number of bytes (or 16-bit words if DFF=16)
 */
void spi_receive(spi_handle_t *handle, uint8_t *data, size_t len);

/**
 * @brief Full-duplex transmit and receive (blocking)
 *
 * Simultaneously sends from tx_data and stores received bytes in rx_data.
 *
 * @param handle   Initialised SPI handle
 * @param tx_data  Data to transmit
 * @param rx_data  Buffer to store received bytes
 * @param len      Number of bytes (or 16-bit words if DFF=16)
 */
void spi_transmit_receive(spi_handle_t *handle, const uint8_t *tx_data,
                          uint8_t *rx_data, size_t len);

/* ── Interrupt-driven transfers ────────────────────────────────────────── */

/**
 * @brief Start an interrupt-driven transmission
 *
 * Copies data into the TX ring buffer and enables the TXE interrupt.
 * Returns immediately — the ISR drains the ring buffer in the
 * background. Calls on_tx_complete when all data has been sent.
 *
 * @param handle  Initialised SPI handle
 * @param data    Data to queue for transmission
 * @param len     Number of bytes (silently truncated to available space)
 * @return Number of bytes actually queued
 */
size_t spi_transmit_it(spi_handle_t *handle, const uint8_t *data,
                       size_t len);

/**
 * @brief Start an interrupt-driven reception
 *
 * Sends dummy bytes to generate clock and stores received data in the
 * RX ring buffer. Calls on_rx_complete when len bytes have been received.
 *
 * @param handle  Initialised SPI handle
 * @param len     Number of bytes to receive
 * @return 0 on success, -1 if already busy
 */
int spi_receive_it(spi_handle_t *handle, size_t len);

/**
 * @brief Read available bytes from the RX ring buffer
 *
 * @param handle   Initialised SPI handle
 * @param data     Destination buffer
 * @param max_len  Maximum number of bytes to read
 * @return Number of bytes actually copied
 */
size_t spi_read_rx_buffer(spi_handle_t *handle, uint8_t *data,
                          size_t max_len);

/**
 * @brief Return the number of unread bytes in the RX ring buffer
 */
size_t spi_rx_available(const spi_handle_t *handle);

/* ── DMA transfers ─────────────────────────────────────────────────────── */

/**
 * @brief Configure the DMA channels used by this SPI
 *
 * Must be called after spi_init() and before any DMA transfer.
 * Enables the DMA1 clock and stores the channel assignments.
 *
 * DMA channel assignments (STM32F103 reference manual Table 78):
 *   SPI1_TX → DMA1 Channel 3 (index 2, IRQ 13)
 *   SPI1_RX → DMA1 Channel 2 (index 1, IRQ 12)
 *   SPI2_TX → DMA1 Channel 5 (index 4, IRQ 15)
 *   SPI2_RX → DMA1 Channel 4 (index 3, IRQ 14)
 *
 * @param handle     Initialised SPI handle
 * @param tx_channel DMA channel for TX (e.g. &DMA1->CH[2] for ch3)
 * @param tx_index   Channel index 0–6 (for ISR/IFCR flag offsets)
 * @param tx_irqn    NVIC IRQ number for the TX DMA channel
 * @param rx_channel DMA channel for RX
 * @param rx_index   Channel index 0–6
 * @param rx_irqn    NVIC IRQ number for the RX DMA channel
 */
void spi_config_dma(spi_handle_t *handle,
                    DMA_Channel_TypeDef *tx_channel, uint8_t tx_index,
                    uint8_t tx_irqn,
                    DMA_Channel_TypeDef *rx_channel, uint8_t rx_index,
                    uint8_t rx_irqn);

/**
 * @brief Transmit a buffer via DMA (non-blocking)
 *
 * The caller must keep the data buffer valid until on_tx_complete fires
 * or spi_dma_tx_handler() reports completion.
 *
 * @param handle  SPI handle with DMA configured
 * @param data    Source buffer (must remain valid until transfer completes)
 * @param len     Number of bytes to transmit
 * @return 0 on success, -1 if DMA is not configured or already busy
 */
int spi_transmit_dma(spi_handle_t *handle, const uint8_t *data,
                     size_t len);

/**
 * @brief Receive a fixed number of bytes via DMA (non-blocking)
 *
 * Configures both TX (sending dummy 0xFF) and RX DMA channels for a
 * receive-only transfer. The caller must provide a valid rx buffer.
 *
 * @param handle  SPI handle with DMA configured
 * @param data    Destination buffer (must remain valid until transfer ends)
 * @param len     Number of bytes to receive
 * @return 0 on success, -1 if DMA is not configured or already busy
 */
int spi_receive_dma(spi_handle_t *handle, uint8_t *data, size_t len);

/**
 * @brief Full-duplex transmit and receive via DMA (non-blocking)
 *
 * @param handle   SPI handle with DMA configured
 * @param tx_data  Source buffer (must remain valid until transfer completes)
 * @param rx_data  Destination buffer (must remain valid until transfer ends)
 * @param len      Number of bytes
 * @return 0 on success, -1 if DMA is not configured or already busy
 */
int spi_transmit_receive_dma(spi_handle_t *handle, const uint8_t *tx_data,
                             uint8_t *rx_data, size_t len);

/* ── ISR entry points (call from vector-table handlers) ────────────────── */

/**
 * @brief SPI interrupt handler
 *
 * Call this from the SPIx_IRQHandler() for the corresponding instance.
 * Handles TXE, RXNE, and error flags.
 */
void spi_irq_handler(spi_handle_t *handle);

/**
 * @brief DMA TX transfer-complete handler
 *
 * Call this from the DMA channel IRQ handler assigned to TX.
 */
void spi_dma_tx_handler(spi_handle_t *handle);

/**
 * @brief DMA RX transfer-complete handler
 *
 * Call this from the DMA channel IRQ handler assigned to RX.
 */
void spi_dma_rx_handler(spi_handle_t *handle);

#endif /* DRIVERS_SPI_H */
