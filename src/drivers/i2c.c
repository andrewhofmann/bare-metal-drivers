/**
 * @file  i2c.c
 * @brief I2C master driver implementation for STM32F1xx
 *
 * Provides polling and interrupt-driven transfer modes for I2C1 and I2C2.
 * Each instance is managed through an i2c_handle_t that the caller
 * allocates and passes to i2c_init().
 *
 * The STM32F1 I2C peripheral has several well-known errata around NACK
 * and STOP timing for short receives (1- and 2-byte). This driver
 * implements the workarounds described in the reference manual (RM0008)
 * and errata sheet (ES096).
 */

#include "drivers/i2c.h"

/* -- Internal helpers ----------------------------------------------------- */

/**
 * @brief Enable the peripheral clock for the specified I2C
 *
 * Both I2C1 and I2C2 are on APB1.
 */
static void i2c_clock_enable(const I2C_TypeDef *i2c)
{
    if (i2c == I2C1) {
        REG_SET(RCC->APB1ENR, RCC_APB1ENR_I2C1EN);
    } else if (i2c == I2C2) {
        REG_SET(RCC->APB1ENR, RCC_APB1ENR_I2C2EN);
    }
}

/**
 * @brief Disable the peripheral clock for the specified I2C
 */
static void i2c_clock_disable(const I2C_TypeDef *i2c)
{
    if (i2c == I2C1) {
        REG_CLR(RCC->APB1ENR, RCC_APB1ENR_I2C1EN);
    } else if (i2c == I2C2) {
        REG_CLR(RCC->APB1ENR, RCC_APB1ENR_I2C2EN);
    }
}

/**
 * @brief Return the event IRQ number for a given I2C instance
 */
static uint32_t i2c_ev_irqn(const I2C_TypeDef *i2c)
{
    if (i2c == I2C1) return I2C1_EV_IRQn;
    return I2C2_EV_IRQn;
}

/**
 * @brief Return the error IRQ number for a given I2C instance
 */
static uint32_t i2c_er_irqn(const I2C_TypeDef *i2c)
{
    if (i2c == I2C1) return I2C1_ER_IRQn;
    return I2C2_ER_IRQn;
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
 * @brief Software timeout counter for blocking waits
 *
 * A simple decrementing counter to prevent infinite spins. At 72 MHz
 * a value of 100000 gives roughly 5-10 ms of timeout depending on
 * compiler optimisation.
 */
#define I2C_TIMEOUT  100000U

/**
 * @brief Wait for a flag in SR1 to be set, with timeout
 * @return 0 on success, -I2C_ERROR_TIMEOUT on timeout
 */
static int i2c_wait_flag(I2C_TypeDef *i2c, uint32_t flag)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (!(i2c->SR1 & flag)) {
        if (--timeout == 0) {
            return -(int)I2C_ERROR_TIMEOUT;
        }
    }
    return 0;
}

/**
 * @brief Generate a START condition and wait for SB
 * @return 0 on success, negative error code on failure
 */
static int i2c_start(I2C_TypeDef *i2c)
{
    REG_SET(i2c->CR1, I2C_CR1_START);
    return i2c_wait_flag(i2c, I2C_SR1_SB);
}

/**
 * @brief Send a 7-bit address byte and wait for ADDR
 *
 * @param i2c   I2C peripheral
 * @param addr  7-bit slave address (unshifted)
 * @param read  true for read direction, false for write
 * @return 0 on success, negative error code on failure
 */
static int i2c_send_addr(I2C_TypeDef *i2c, uint8_t addr, bool read)
{
    i2c->DR = (uint32_t)((addr << 1) | (read ? 1U : 0U));

    uint32_t timeout = I2C_TIMEOUT;
    while (!(i2c->SR1 & I2C_SR1_ADDR)) {
        if (i2c->SR1 & I2C_SR1_AF) {
            /* NACK on address — generate STOP and clear AF */
            REG_SET(i2c->CR1, I2C_CR1_STOP);
            REG_CLR(i2c->SR1, I2C_SR1_AF);
            return -(int)I2C_ERROR_AF;
        }
        if (--timeout == 0) {
            return -(int)I2C_ERROR_TIMEOUT;
        }
    }

    /* Clear ADDR by reading SR1 then SR2 */
    (void)i2c->SR1;
    (void)i2c->SR2;
    return 0;
}

/**
 * @brief Generate a STOP condition
 */
static inline void i2c_stop(I2C_TypeDef *i2c)
{
    REG_SET(i2c->CR1, I2C_CR1_STOP);
}

/* -- Initialisation / deinitialisation ------------------------------------ */

void i2c_init(i2c_handle_t *handle, I2C_TypeDef *i2c,
              const i2c_config_t *config)
{
    handle->instance = i2c;
    handle->config   = *config;
    handle->state    = I2C_STATE_RESET;
    handle->errors   = I2C_ERROR_NONE;

    handle->buf         = (void *)0;
    handle->xfer_size   = 0;
    handle->xfer_count  = 0;
    handle->addr        = 0;

    handle->on_tx_complete = (void *)0;
    handle->on_rx_complete = (void *)0;
    handle->on_error       = (void *)0;

    /* Enable peripheral clock */
    i2c_clock_enable(i2c);

    /* Disable peripheral for configuration */
    REG_CLR(i2c->CR1, I2C_CR1_PE);

    /* Software reset to clear any wedged state */
    REG_SET(i2c->CR1, I2C_CR1_SWRST);
    REG_CLR(i2c->CR1, I2C_CR1_SWRST);

    /*
     * CR2 FREQ field: APB1 clock frequency in MHz.
     * Must be at least 2 MHz for standard mode, 4 MHz for fast mode.
     */
    uint32_t freq_mhz = config->pclk1_hz / 1000000UL;
    REG_MODIFY(i2c->CR2, I2C_CR2_FREQ_MASK, freq_mhz & I2C_CR2_FREQ_MASK);

    /*
     * CCR: clock control register
     *
     * Standard mode (100 kHz):
     *   CCR = f_pclk1 / (2 * 100000)
     *   TRISE = (f_pclk1 / 1000000) + 1   (1000 ns max rise time)
     *
     * Fast mode (400 kHz):
     *   DUTY=0: CCR = f_pclk1 / (3 * 400000)
     *   DUTY=1: CCR = f_pclk1 / (25 * 400000)
     *   TRISE = (f_pclk1 * 300 / 1000000000) + 1   (300 ns max rise time)
     */
    uint32_t ccr_val;

    if (config->speed == I2C_SPEED_STANDARD) {
        ccr_val = config->pclk1_hz / (2UL * 100000UL);
        if (ccr_val < 4) {
            ccr_val = 4;    /* Minimum value in standard mode */
        }
        i2c->CCR = ccr_val & I2C_CCR_CCR_MASK;
        i2c->TRISE = freq_mhz + 1UL;
    } else {
        /* Fast mode */
        if (config->duty == I2C_DUTY_2) {
            ccr_val = config->pclk1_hz / (3UL * 400000UL);
            if (ccr_val < 1) {
                ccr_val = 1;
            }
            i2c->CCR = I2C_CCR_FS | (ccr_val & I2C_CCR_CCR_MASK);
        } else {
            ccr_val = config->pclk1_hz / (25UL * 400000UL);
            if (ccr_val < 1) {
                ccr_val = 1;
            }
            i2c->CCR = I2C_CCR_FS | I2C_CCR_DUTY
                     | (ccr_val & I2C_CCR_CCR_MASK);
        }
        i2c->TRISE = (freq_mhz * 300UL) / 1000UL + 1UL;
    }

    /* Enable peripheral */
    REG_SET(i2c->CR1, I2C_CR1_PE);

    /* Enable ACK by default for multi-byte receives */
    REG_SET(i2c->CR1, I2C_CR1_ACK);

    handle->state = I2C_STATE_READY;
}

void i2c_deinit(i2c_handle_t *handle)
{
    I2C_TypeDef *i2c = handle->instance;

    /* Disable all interrupts */
    REG_CLR(i2c->CR2, I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);

    /* Disable peripheral */
    REG_CLR(i2c->CR1, I2C_CR1_PE);

    /* Disable NVIC IRQs */
    nvic_disable_irq(i2c_ev_irqn(i2c));
    nvic_disable_irq(i2c_er_irqn(i2c));

    /* Disable peripheral clock */
    i2c_clock_disable(i2c);

    handle->state = I2C_STATE_RESET;
}

/* -- Polling (blocking) transfers ----------------------------------------- */

int i2c_master_transmit(i2c_handle_t *handle, uint8_t addr,
                        const uint8_t *data, size_t len)
{
    I2C_TypeDef *i2c = handle->instance;
    int rc;

    /* Wait until the bus is free */
    uint32_t timeout = I2C_TIMEOUT;
    while (i2c->SR2 & I2C_SR2_BUSY) {
        if (--timeout == 0) return -(int)I2C_ERROR_TIMEOUT;
    }

    /* Generate START */
    rc = i2c_start(i2c);
    if (rc != 0) return rc;

    /* Send slave address + write */
    rc = i2c_send_addr(i2c, addr, false);
    if (rc != 0) return rc;

    /* Transmit data bytes */
    for (size_t i = 0; i < len; i++) {
        rc = i2c_wait_flag(i2c, I2C_SR1_TXE);
        if (rc != 0) {
            i2c_stop(i2c);
            return rc;
        }
        i2c->DR = data[i];
    }

    /* Wait for the last byte to finish shifting out */
    rc = i2c_wait_flag(i2c, I2C_SR1_BTF);
    if (rc != 0) {
        i2c_stop(i2c);
        return rc;
    }

    /* Generate STOP */
    i2c_stop(i2c);

    return 0;
}

int i2c_master_receive(i2c_handle_t *handle, uint8_t addr,
                       uint8_t *data, size_t len)
{
    I2C_TypeDef *i2c = handle->instance;
    int rc;

    if (len == 0) return 0;

    /* Wait until the bus is free */
    uint32_t timeout = I2C_TIMEOUT;
    while (i2c->SR2 & I2C_SR2_BUSY) {
        if (--timeout == 0) return -(int)I2C_ERROR_TIMEOUT;
    }

    /* Enable ACK for multi-byte receives */
    REG_SET(i2c->CR1, I2C_CR1_ACK);

    /* Generate START */
    rc = i2c_start(i2c);
    if (rc != 0) return rc;

    if (len == 1) {
        /*
         * Single-byte receive (RM0008 errata workaround):
         * Disable ACK before clearing ADDR, then set STOP.
         */
        i2c->DR = (uint32_t)((addr << 1) | 1U);
        rc = i2c_wait_flag(i2c, I2C_SR1_ADDR);
        if (rc != 0) return rc;

        REG_CLR(i2c->CR1, I2C_CR1_ACK);

        /* Clear ADDR (read SR1 + SR2) */
        (void)i2c->SR1;
        (void)i2c->SR2;

        i2c_stop(i2c);

        rc = i2c_wait_flag(i2c, I2C_SR1_RXNE);
        if (rc != 0) return rc;
        data[0] = (uint8_t)(i2c->DR & 0xFFU);

    } else if (len == 2) {
        /*
         * Two-byte receive (RM0008 errata workaround):
         * Set POS and ACK before ADDR clear. After ADDR clear,
         * clear ACK so the NACK is applied to the second byte.
         */
        REG_SET(i2c->CR1, I2C_CR1_POS);

        i2c->DR = (uint32_t)((addr << 1) | 1U);
        rc = i2c_wait_flag(i2c, I2C_SR1_ADDR);
        if (rc != 0) return rc;

        /* Clear ADDR */
        (void)i2c->SR1;
        (void)i2c->SR2;

        REG_CLR(i2c->CR1, I2C_CR1_ACK);

        /* Wait until both bytes are received (BTF set) */
        rc = i2c_wait_flag(i2c, I2C_SR1_BTF);
        if (rc != 0) return rc;

        i2c_stop(i2c);

        data[0] = (uint8_t)(i2c->DR & 0xFFU);
        data[1] = (uint8_t)(i2c->DR & 0xFFU);

        REG_CLR(i2c->CR1, I2C_CR1_POS);

    } else {
        /*
         * N >= 3 byte receive: straightforward read loop.
         * NACK the last byte and generate STOP.
         */
        rc = i2c_send_addr(i2c, addr, true);
        if (rc != 0) return rc;

        for (size_t i = 0; i < len; i++) {
            if (i == len - 1) {
                /* Last byte: disable ACK and generate STOP before read */
                REG_CLR(i2c->CR1, I2C_CR1_ACK);
                i2c_stop(i2c);
            }

            rc = i2c_wait_flag(i2c, I2C_SR1_RXNE);
            if (rc != 0) return rc;
            data[i] = (uint8_t)(i2c->DR & 0xFFU);
        }
    }

    return 0;
}

int i2c_mem_write(i2c_handle_t *handle, uint8_t addr, uint8_t reg,
                  const uint8_t *data, size_t len)
{
    I2C_TypeDef *i2c = handle->instance;
    int rc;

    /* Wait until the bus is free */
    uint32_t timeout = I2C_TIMEOUT;
    while (i2c->SR2 & I2C_SR2_BUSY) {
        if (--timeout == 0) return -(int)I2C_ERROR_TIMEOUT;
    }

    /* Generate START */
    rc = i2c_start(i2c);
    if (rc != 0) return rc;

    /* Send slave address + write */
    rc = i2c_send_addr(i2c, addr, false);
    if (rc != 0) return rc;

    /* Send register address */
    rc = i2c_wait_flag(i2c, I2C_SR1_TXE);
    if (rc != 0) {
        i2c_stop(i2c);
        return rc;
    }
    i2c->DR = reg;

    /* Transmit data bytes */
    for (size_t i = 0; i < len; i++) {
        rc = i2c_wait_flag(i2c, I2C_SR1_TXE);
        if (rc != 0) {
            i2c_stop(i2c);
            return rc;
        }
        i2c->DR = data[i];
    }

    /* Wait for last byte to finish */
    rc = i2c_wait_flag(i2c, I2C_SR1_BTF);
    if (rc != 0) {
        i2c_stop(i2c);
        return rc;
    }

    /* Generate STOP */
    i2c_stop(i2c);

    return 0;
}

int i2c_mem_read(i2c_handle_t *handle, uint8_t addr, uint8_t reg,
                 uint8_t *data, size_t len)
{
    I2C_TypeDef *i2c = handle->instance;
    int rc;

    if (len == 0) return 0;

    /* Wait until the bus is free */
    uint32_t timeout = I2C_TIMEOUT;
    while (i2c->SR2 & I2C_SR2_BUSY) {
        if (--timeout == 0) return -(int)I2C_ERROR_TIMEOUT;
    }

    /*
     * Phase 1: Write the register address.
     * START → addr+W → register byte (no STOP — repeated START follows)
     */
    rc = i2c_start(i2c);
    if (rc != 0) return rc;

    rc = i2c_send_addr(i2c, addr, false);
    if (rc != 0) return rc;

    rc = i2c_wait_flag(i2c, I2C_SR1_TXE);
    if (rc != 0) {
        i2c_stop(i2c);
        return rc;
    }
    i2c->DR = reg;

    /* Wait for the register address byte to finish */
    rc = i2c_wait_flag(i2c, I2C_SR1_BTF);
    if (rc != 0) {
        i2c_stop(i2c);
        return rc;
    }

    /*
     * Phase 2: Repeated START and read.
     * Re-use the master_receive logic for proper 1/2/N byte handling.
     */
    return i2c_master_receive(handle, addr, data, len);
}

/* -- Interrupt-driven transfers ------------------------------------------- */

int i2c_master_transmit_it(i2c_handle_t *handle, uint8_t addr,
                           const uint8_t *data, size_t len)
{
    if (handle->state != I2C_STATE_READY) {
        return -1;
    }

    handle->addr        = addr;
    handle->buf         = (uint8_t *)(uintptr_t)data;
    handle->xfer_size   = len;
    handle->xfer_count  = len;
    handle->errors      = I2C_ERROR_NONE;
    handle->state       = I2C_STATE_BUSY_TX;

    I2C_TypeDef *i2c = handle->instance;

    /* Enable event, buffer, and error interrupts */
    REG_SET(i2c->CR2, I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);

    nvic_enable_irq(i2c_ev_irqn(i2c));
    nvic_enable_irq(i2c_er_irqn(i2c));

    /* Generate START — the event ISR handles the rest */
    REG_SET(i2c->CR1, I2C_CR1_START);

    return 0;
}

int i2c_master_receive_it(i2c_handle_t *handle, uint8_t addr,
                          uint8_t *data, size_t len)
{
    if (handle->state != I2C_STATE_READY || len == 0) {
        return -1;
    }

    handle->addr        = addr;
    handle->buf         = data;
    handle->xfer_size   = len;
    handle->xfer_count  = len;
    handle->errors      = I2C_ERROR_NONE;
    handle->state       = I2C_STATE_BUSY_RX;

    I2C_TypeDef *i2c = handle->instance;

    /* Enable ACK for multi-byte receives */
    REG_SET(i2c->CR1, I2C_CR1_ACK);

    /* Enable event, buffer, and error interrupts */
    REG_SET(i2c->CR2, I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);

    nvic_enable_irq(i2c_ev_irqn(i2c));
    nvic_enable_irq(i2c_er_irqn(i2c));

    /* Generate START — the event ISR handles the rest */
    REG_SET(i2c->CR1, I2C_CR1_START);

    return 0;
}

/* -- ISR entry points ----------------------------------------------------- */

void i2c_ev_irq_handler(i2c_handle_t *handle)
{
    I2C_TypeDef *i2c = handle->instance;
    uint32_t sr1 = i2c->SR1;

    /* -- SB: Start condition generated ------------------------------------ */
    if (sr1 & I2C_SR1_SB) {
        if (handle->state == I2C_STATE_BUSY_TX) {
            i2c->DR = (uint32_t)(handle->addr << 1);       /* Write */
        } else {
            i2c->DR = (uint32_t)((handle->addr << 1) | 1U); /* Read */
        }
        return;
    }

    /* -- ADDR: Address sent ----------------------------------------------- */
    if (sr1 & I2C_SR1_ADDR) {
        if (handle->state == I2C_STATE_BUSY_RX && handle->xfer_count == 1) {
            /* Single byte: disable ACK before ADDR clear */
            REG_CLR(i2c->CR1, I2C_CR1_ACK);
        }
        /* Clear ADDR by reading SR1 + SR2 */
        (void)i2c->SR1;
        (void)i2c->SR2;

        if (handle->state == I2C_STATE_BUSY_RX && handle->xfer_count == 1) {
            /* Generate STOP immediately for 1-byte receive */
            i2c_stop(i2c);
        }
        return;
    }

    /* -- TXE: Transmit buffer empty (TX mode) ----------------------------- */
    if ((sr1 & I2C_SR1_TXE) && handle->state == I2C_STATE_BUSY_TX) {
        if (handle->xfer_count > 0) {
            size_t idx = handle->xfer_size - handle->xfer_count;
            i2c->DR = handle->buf[idx];
            handle->xfer_count--;
        } else {
            /*
             * All data sent. Wait for BTF to ensure the last byte has
             * been fully shifted out, then generate STOP.
             */
            if (sr1 & I2C_SR1_BTF) {
                i2c_stop(i2c);

                /* Disable interrupts */
                REG_CLR(i2c->CR2,
                        I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);

                handle->state = I2C_STATE_READY;

                if (handle->on_tx_complete) {
                    handle->on_tx_complete(handle);
                }
            }
        }
        return;
    }

    /* -- RXNE: Data register not empty (RX mode) -------------------------- */
    if ((sr1 & I2C_SR1_RXNE) && handle->state == I2C_STATE_BUSY_RX) {
        size_t idx = handle->xfer_size - handle->xfer_count;
        handle->buf[idx] = (uint8_t)(i2c->DR & 0xFFU);
        handle->xfer_count--;

        if (handle->xfer_count == 1) {
            /* Next byte is the last: disable ACK, set STOP */
            REG_CLR(i2c->CR1, I2C_CR1_ACK);
            i2c_stop(i2c);
        }

        if (handle->xfer_count == 0) {
            /* All bytes received */
            REG_CLR(i2c->CR2,
                    I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);

            handle->state = I2C_STATE_READY;

            if (handle->on_rx_complete) {
                handle->on_rx_complete(handle);
            }
        }
        return;
    }
}

void i2c_er_irq_handler(i2c_handle_t *handle)
{
    I2C_TypeDef *i2c = handle->instance;
    uint32_t sr1 = i2c->SR1;

    /* Bus error: misplaced START/STOP detected by hardware */
    if (sr1 & I2C_SR1_BERR) {
        handle->errors |= I2C_ERROR_BERR;
        REG_CLR(i2c->SR1, I2C_SR1_BERR);
    }

    /* Arbitration lost: another master won the bus */
    if (sr1 & I2C_SR1_ARLO) {
        handle->errors |= I2C_ERROR_ARLO;
        REG_CLR(i2c->SR1, I2C_SR1_ARLO);
    }

    /* Acknowledge failure: slave did not ACK */
    if (sr1 & I2C_SR1_AF) {
        handle->errors |= I2C_ERROR_AF;
        REG_CLR(i2c->SR1, I2C_SR1_AF);
        /* Generate STOP to release the bus */
        i2c_stop(i2c);
    }

    /* Overrun/underrun */
    if (sr1 & I2C_SR1_OVR) {
        handle->errors |= I2C_ERROR_OVR;
        REG_CLR(i2c->SR1, I2C_SR1_OVR);
    }

    /* Disable interrupts and return to ready */
    REG_CLR(i2c->CR2, I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
    handle->state = I2C_STATE_READY;

    if (handle->on_error) {
        handle->on_error(handle);
    }
}
