#ifndef I2C_SLAVE_MINIMAL_H
#define I2C_SLAVE_MINIMAL_H

#include <stdint.h>

#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/memorymap.h>
#include <libopencm3/py32/f0/nvic.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>

/* Define I2C_SLAVE_ADDRESS before including this file to change it. */
#ifndef I2C_SLAVE_ADDRESS
#define I2C_SLAVE_ADDRESS       0x20U
#endif

#define I2C_BUS_FREQUENCY_HZ    100000U
#define I2C_WRITE_BUFFER_SIZE   4U
#define I2C_READ_BUFFER_SIZE    2U

#if I2C_SLAVE_ADDRESS < 0x08U || I2C_SLAVE_ADDRESS > 0x77U
#error "I2C_SLAVE_ADDRESS must be a 7-bit address from 0x08 to 0x77"
#endif

#define I2C_CR1_REG             MMIO32(I2C1_BASE + 0x00U)
#define I2C_CR2_REG             MMIO32(I2C1_BASE + 0x04U)
#define I2C_OAR1_REG            MMIO32(I2C1_BASE + 0x08U)
#define I2C_DR_REG              MMIO32(I2C1_BASE + 0x10U)
#define I2C_SR1_REG             MMIO32(I2C1_BASE + 0x14U)
#define I2C_SR2_REG             MMIO32(I2C1_BASE + 0x18U)
#define I2C_CCR_REG             MMIO32(I2C1_BASE + 0x1CU)
#define I2C_TRISE_REG           MMIO32(I2C1_BASE + 0x20U)

#define I2C_CR1_PE_BIT          (1U << 0)
#define I2C_CR1_ACK_BIT         (1U << 10)
#define I2C_CR2_ITERREN_BIT     (1U << 8)
#define I2C_CR2_ITEVTEN_BIT     (1U << 9)
#define I2C_CR2_ITBUFEN_BIT     (1U << 10)
#define I2C_SR1_ADDR_BIT        (1U << 1)
#define I2C_SR1_BTF_BIT         (1U << 2)
#define I2C_SR1_STOPF_BIT       (1U << 4)
#define I2C_SR1_RXNE_BIT        (1U << 6)
#define I2C_SR1_TXE_BIT         (1U << 7)
#define I2C_SR1_BERR_BIT        (1U << 8)
#define I2C_SR1_ARLO_BIT        (1U << 9)
#define I2C_SR1_AF_BIT          (1U << 10)
#define I2C_SR1_OVR_BIT         (1U << 11)
#define I2C_SR1_PECERR_BIT      (1U << 12)
#define I2C_SR2_TRA_BIT         (1U << 2)
#define I2C_ERROR_BITS          (I2C_SR1_BERR_BIT | I2C_SR1_ARLO_BIT | \
                                 I2C_SR1_OVR_BIT | I2C_SR1_PECERR_BIT)

#define SYSCFG_CFGR1_REG        MMIO32(SYSCFG_BASE + 0x00U)
#define SYSCFG_I2C_PA10_ANF     (1U << 23)
#define SYSCFG_I2C_PB6_ANF      (1U << 26)

static volatile uint8_t i2c_write_buffer[I2C_WRITE_BUFFER_SIZE];
static volatile uint8_t i2c_write_length;
static volatile uint8_t i2c_write_ready;
static volatile uint8_t i2c_write_overflow;
static volatile uint8_t i2c_read_buffer[I2C_READ_BUFFER_SIZE] = {0xFFU, 0xFFU};
static volatile uint8_t i2c_read_length = 1U;
static volatile uint8_t i2c_read_index;

static void i2c_set_response(const uint8_t *data, uint8_t length)
{
    uint8_t i;

    if (length == 0U || length > I2C_READ_BUFFER_SIZE) {
        return;
    }

    cm_disable_interrupts();
    for (i = 0U; i < length; i++) {
        i2c_read_buffer[i] = data[i];
    }
    i2c_read_length = length;
    i2c_read_index = 0U;
    cm_enable_interrupts();
}

static void i2c_set_response_u8(uint8_t value)
{
    i2c_set_response(&value, 1U);
}

static inline void i2c_set_response_u16_le(uint16_t value)
{
    uint8_t response[2];

    response[0] = (uint8_t)value;
    response[1] = (uint8_t)(value >> 8);
    i2c_set_response(response, 2U);
}

/* Returns the byte count from the most recent completed write transaction. */
static uint8_t i2c_take_write(uint8_t *data, uint8_t capacity)
{
    uint8_t length = 0U;
    uint8_t i;

    cm_disable_interrupts();
    if (i2c_write_ready != 0U) {
        length = i2c_write_length;
        if (length > capacity) {
            length = capacity;
        }
        for (i = 0U; i < length; i++) {
            data[i] = i2c_write_buffer[i];
        }
        i2c_write_ready = 0U;
    }
    cm_enable_interrupts();
    return length;
}

void i2c1_isr(void)
{
    uint32_t status = I2C_SR1_REG;

    if ((status & I2C_ERROR_BITS) != 0U) {
        I2C_SR1_REG &= ~(status & I2C_ERROR_BITS);
        i2c_write_length = 0U;
        i2c_write_overflow = 0U;
    }

    /* AF is the normal end of a slave transmit after the master NACKs. */
    if ((status & I2C_SR1_AF_BIT) != 0U) {
        I2C_SR1_REG &= ~I2C_SR1_AF_BIT;
        I2C_CR2_REG &= ~I2C_CR2_ITBUFEN_BIT;
        status = I2C_SR1_REG;
    }

    if ((status & I2C_SR1_ADDR_BIT) != 0U) {
        uint32_t direction = I2C_SR2_REG; /* SR1 then SR2 clears ADDR. */

        I2C_CR2_REG |= I2C_CR2_ITBUFEN_BIT;
        if ((direction & I2C_SR2_TRA_BIT) != 0U) {
            i2c_read_index = 0U;
            I2C_DR_REG = i2c_read_buffer[i2c_read_index++];
        } else {
            i2c_write_length = 0U;
            i2c_write_overflow = 0U;
        }
        return;
    }

    if ((status & I2C_SR1_RXNE_BIT) != 0U) {
        uint8_t value = (uint8_t)I2C_DR_REG;

        if (i2c_write_length < I2C_WRITE_BUFFER_SIZE) {
            i2c_write_buffer[i2c_write_length++] = value;
        } else {
            i2c_write_overflow = 1U;
        }
        status = I2C_SR1_REG;
    }

    if ((status & I2C_SR1_STOPF_BIT) != 0U) {
        (void)I2C_SR1_REG;
        I2C_CR1_REG |= I2C_CR1_PE_BIT | I2C_CR1_ACK_BIT;
        I2C_CR2_REG &= ~I2C_CR2_ITBUFEN_BIT;
        if (i2c_write_length != 0U && i2c_write_overflow == 0U) {
            i2c_write_ready = 1U;
        }
        return;
    }

    if ((status & (I2C_SR1_TXE_BIT | I2C_SR1_BTF_BIT)) != 0U) {
        I2C_DR_REG = (i2c_read_index < i2c_read_length)
                         ? i2c_read_buffer[i2c_read_index++]
                         : 0xFFU;
    }
}

static void i2c_slave_setup(void)
{
    uint32_t peripheral_clock_mhz = rcc_apb_frequency / 1000000U;

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_SYSCFG);
    rcc_periph_clock_enable(RCC_I2C1);

    /* PB6=SCL and PA10=SDA use AF6. Disable their SWD pad aliases. */
    gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO13 | GPIO14);

    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_HIGH, GPIO6);
    gpio_set_af(GPIOB, GPIO_AF6, GPIO6);

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO10);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_HIGH, GPIO10);
    gpio_set_af(GPIOA, GPIO_AF6, GPIO10);

    SYSCFG_CFGR1_REG |= SYSCFG_I2C_PA10_ANF | SYSCFG_I2C_PB6_ANF;

    I2C_CR1_REG = 0U;
    I2C_CR2_REG = peripheral_clock_mhz;
    I2C_OAR1_REG = I2C_SLAVE_ADDRESS << 1;
    I2C_CCR_REG = rcc_apb_frequency / (2U * I2C_BUS_FREQUENCY_HZ);
    I2C_TRISE_REG = peripheral_clock_mhz + 1U;
    I2C_CR1_REG = I2C_CR1_PE_BIT | I2C_CR1_ACK_BIT;
    I2C_CR2_REG |= I2C_CR2_ITEVTEN_BIT | I2C_CR2_ITERREN_BIT;

    nvic_enable_irq(NVIC_I2C1_IRQ);
}

#endif
