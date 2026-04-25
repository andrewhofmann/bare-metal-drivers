/**
 * @file  i2c.h
 * @brief I2C master driver interface for STM32F1xx
 *
 * Supports two transfer modes:
 *   - Polling:    blocking single and multi-byte read/write
 *   - Interrupt:  non-blocking multi-byte transfers with callbacks
 *
 * Both standard mode (100 kHz) and fast mode (400 kHz) are supported.
 * Each I2C instance is managed through an i2c_handle_t that tracks
 * peripheral state, transfer buffers, and user callbacks.
 */

#ifndef DRIVERS_I2C_H
#define DRIVERS_I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f1xx.h"

/* -- Enumerations --------------------------------------------------------- */

/**
 * @brief I2C speed mode
 */
typedef enum {
    I2C_SPEED_STANDARD = 0,     /**< 100 kHz (Sm)  */
    I2C_SPEED_FAST     = 1,     /**< 400 kHz (Fm)  */
} i2c_speed_t;

/**
 * @brief Fast-mode duty cycle selection
 *
 * Only meaningful when speed is I2C_SPEED_FAST.
 *   DUTY_2:    T_low / T_high = 2
 *   DUTY_16_9: T_low / T_high = 16/9 (wider high pulse)
 */
typedef enum {
    I2C_DUTY_2    = 0,          /**< T_low/T_high = 2     */
    I2C_DUTY_16_9 = 1,          /**< T_low/T_high = 16/9  */
} i2c_duty_t;

/**
 * @brief Current state of an I2C handle
 */
typedef enum {
    I2C_STATE_RESET = 0,        /**< Not initialised               */
    I2C_STATE_READY,            /**< Idle, ready for transfer       */
    I2C_STATE_BUSY_TX,          /**< Non-blocking TX in progress    */
    I2C_STATE_BUSY_RX,          /**< Non-blocking RX in progress    */
} i2c_state_t;

/**
 * @brief Error flags (can be ORed together)
 */
typedef enum {
    I2C_ERROR_NONE    = 0x00,
    I2C_ERROR_BERR    = 0x01,   /**< Bus error                     */
    I2C_ERROR_ARLO    = 0x02,   /**< Arbitration lost              */
    I2C_ERROR_AF      = 0x04,   /**< Acknowledge failure           */
    I2C_ERROR_OVR     = 0x08,   /**< Overrun/underrun              */
    I2C_ERROR_TIMEOUT = 0x10,   /**< Timeout (software)            */
} i2c_error_t;

/* -- Structures ----------------------------------------------------------- */

/**
 * @brief Static I2C configuration applied during i2c_init()
 */
typedef struct {
    i2c_speed_t  speed;         /**< Standard (100 kHz) or fast (400 kHz) */
    i2c_duty_t   duty;          /**< Duty cycle for fast mode             */
    uint32_t     pclk1_hz;      /**< APB1 peripheral clock in Hz          */
} i2c_config_t;

/** Forward declaration for callback signatures */
typedef struct i2c_handle i2c_handle_t;

/**
 * @brief User callback type for transfer-complete and error events
 */
typedef void (*i2c_callback_t)(i2c_handle_t *handle);

/**
 * @brief I2C handle -- one per I2C instance
 *
 * Allocate statically and pass to i2c_init(). Do not modify fields
 * directly after initialisation -- use the driver API.
 */
struct i2c_handle {
    I2C_TypeDef          *instance;     /**< I2C peripheral pointer          */
    i2c_config_t          config;       /**< Active configuration            */
    volatile i2c_state_t  state;        /**< Transfer state machine          */
    volatile i2c_error_t  errors;       /**< Accumulated error flags         */

    /* Interrupt-mode transfer tracking */
    uint8_t               addr;         /**< Target slave address (7-bit)    */
    uint8_t              *buf;          /**< Pointer to user TX/RX buffer    */
    volatile size_t       xfer_size;    /**< Total transfer length           */
    volatile size_t       xfer_count;   /**< Bytes remaining                 */

    /* User callbacks -- NULL means no notification */
    i2c_callback_t  on_tx_complete;     /**< All TX data sent                */
    i2c_callback_t  on_rx_complete;     /**< RX transfer finished            */
    i2c_callback_t  on_error;           /**< Error detected during transfer  */
};

/* -- Initialisation / deinitialisation ------------------------------------ */

/**
 * @brief Initialise an I2C peripheral in master mode
 *
 * Enables the peripheral clock, configures the clock speed (CCR and TRISE),
 * and enables the peripheral. The caller must enable GPIO clocks and
 * configure SDA/SCL pins as alternate-function open-drain before calling.
 *
 * @param handle  Pointer to a caller-allocated handle
 * @param i2c     I2C peripheral (I2C1 or I2C2)
 * @param config  Desired configuration
 */
void i2c_init(i2c_handle_t *handle, I2C_TypeDef *i2c,
              const i2c_config_t *config);

/**
 * @brief Disable the I2C peripheral and reset the handle
 */
void i2c_deinit(i2c_handle_t *handle);

/* -- Polling (blocking) transfers ----------------------------------------- */

/**
 * @brief Write a buffer of bytes to a slave device (blocking)
 *
 * Generates START, sends the 7-bit slave address with write bit,
 * transmits all bytes, then generates STOP.
 *
 * @param handle  Initialised I2C handle
 * @param addr    7-bit slave address (unshifted)
 * @param data    Data to transmit
 * @param len     Number of bytes to transmit
 * @return 0 on success, negative i2c_error_t on failure
 */
int i2c_master_transmit(i2c_handle_t *handle, uint8_t addr,
                        const uint8_t *data, size_t len);

/**
 * @brief Read a buffer of bytes from a slave device (blocking)
 *
 * Generates START, sends the 7-bit slave address with read bit,
 * receives all bytes with ACK (NACK on last byte), then generates STOP.
 *
 * @param handle  Initialised I2C handle
 * @param addr    7-bit slave address (unshifted)
 * @param data    Buffer to store received data
 * @param len     Number of bytes to receive (must be >= 1)
 * @return 0 on success, negative i2c_error_t on failure
 */
int i2c_master_receive(i2c_handle_t *handle, uint8_t addr,
                       uint8_t *data, size_t len);

/**
 * @brief Write to a register on a slave device (blocking)
 *
 * Combines a register-address write and a data write into a single
 * I2C transaction (no repeated START). Useful for multi-byte writes
 * to devices that auto-increment their internal address pointer.
 *
 * @param handle   Initialised I2C handle
 * @param addr     7-bit slave address (unshifted)
 * @param reg      Register address to write to
 * @param data     Data to write starting at reg
 * @param len      Number of data bytes
 * @return 0 on success, negative i2c_error_t on failure
 */
int i2c_mem_write(i2c_handle_t *handle, uint8_t addr, uint8_t reg,
                  const uint8_t *data, size_t len);

/**
 * @brief Read from a register on a slave device (blocking)
 *
 * Sends the register address with a write, then issues a repeated START
 * to read the requested number of bytes. Useful for multi-byte reads
 * from devices that auto-increment their internal address pointer.
 *
 * @param handle   Initialised I2C handle
 * @param addr     7-bit slave address (unshifted)
 * @param reg      Register address to read from
 * @param data     Buffer to store received data
 * @param len      Number of bytes to read (must be >= 1)
 * @return 0 on success, negative i2c_error_t on failure
 */
int i2c_mem_read(i2c_handle_t *handle, uint8_t addr, uint8_t reg,
                 uint8_t *data, size_t len);

/* -- Interrupt-driven transfers ------------------------------------------- */

/**
 * @brief Start an interrupt-driven write to a slave device
 *
 * Returns immediately. The ISR handles address transmission and
 * data byte pumping. Calls on_tx_complete when all data has been sent.
 * The caller must keep the data buffer valid until the callback fires.
 *
 * @param handle  Initialised I2C handle
 * @param addr    7-bit slave address (unshifted)
 * @param data    Data to transmit (must remain valid until completion)
 * @param len     Number of bytes to transmit
 * @return 0 on success, -1 if already busy
 */
int i2c_master_transmit_it(i2c_handle_t *handle, uint8_t addr,
                           const uint8_t *data, size_t len);

/**
 * @brief Start an interrupt-driven read from a slave device
 *
 * Returns immediately. The ISR handles address transmission, ACK/NACK
 * control, and data reception. Calls on_rx_complete when all data has
 * been received.
 *
 * @param handle  Initialised I2C handle
 * @param addr    7-bit slave address (unshifted)
 * @param data    Buffer to store received data (must remain valid)
 * @param len     Number of bytes to receive (must be >= 1)
 * @return 0 on success, -1 if already busy
 */
int i2c_master_receive_it(i2c_handle_t *handle, uint8_t addr,
                          uint8_t *data, size_t len);

/* -- ISR entry points (call from vector-table handlers) ------------------- */

/**
 * @brief I2C event interrupt handler
 *
 * Call this from the I2Cx_EV_IRQHandler() for the corresponding instance.
 * Handles SB, ADDR, TXE, RXNE, and BTF events.
 */
void i2c_ev_irq_handler(i2c_handle_t *handle);

/**
 * @brief I2C error interrupt handler
 *
 * Call this from the I2Cx_ER_IRQHandler() for the corresponding instance.
 * Handles BERR, ARLO, AF, and OVR flags.
 */
void i2c_er_irq_handler(i2c_handle_t *handle);

#endif /* DRIVERS_I2C_H */
