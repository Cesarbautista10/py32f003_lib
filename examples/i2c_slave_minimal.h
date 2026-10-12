#ifndef I2C_SLAVE_MINIMAL_H
#define I2C_SLAVE_MINIMAL_H

#include <stdint.h>

#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/memorymap.h>
#include <libopencm3/py32/f0/nvic.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>

/* Define I2C_SLAVE_ADDRESS to change the address.
 * I2C_SLAVE_PF0_PF1 selects PF0/PF1; otherwise PA10/PB6 are used. */
#ifndef I2C_SLAVE_ADDRESS
#define I2C_SLAVE_ADDRESS       0x20U
#endif

#define I2C_BUS_FREQUENCY_HZ    100000U
#ifndef I2C_WRITE_BUFFER_SIZE
#define I2C_WRITE_BUFFER_SIZE   4U
#endif
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
#define I2C_CR1_SWRST_BIT       (1U << 15)
#define I2C_CR2_ITERREN_BIT     (1U << 8)
#define I2C_CR2_ITEVTEN_BIT     (1U << 9)
#define I2C_CR2_ITBUFEN_BIT     (1U << 10)
#define I2C_SR1_ADDR_BIT        (1U << 1)
#define I2C_SR1_STOPF_BIT       (1U << 4)
#define I2C_SR1_RXNE_BIT        (1U << 6)
#define I2C_SR1_TXE_BIT         (1U << 7)
#define I2C_SR1_BERR_BIT        (1U << 8)
#define I2C_SR1_ARLO_BIT        (1U << 9)
#define I2C_SR1_AF_BIT          (1U << 10)
#define I2C_SR1_OVR_BIT         (1U << 11)
#define I2C_SR1_PECERR_BIT      (1U << 12)
#define I2C_SR2_TRA_BIT         (1U << 2)
#define I2C_CCR_FS_BIT          (1U << 15)
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

/* Register-level setup adapted from the proven TEMT6000 direct-I2C driver. */
static void i2c_direct_configure(void)
{
    uint32_t peripheral_clock_mhz = rcc_apb_frequency / 1000000U;
    uint32_t ccr;

    if (peripheral_clock_mhz < 2U) {
        peripheral_clock_mhz = 2U;
    }

    I2C_CR1_REG = 0U;
    I2C_CR2_REG = peripheral_clock_mhz;
    I2C_OAR1_REG = (I2C_SLAVE_ADDRESS << 1) & 0xFEU;

    if (I2C_BUS_FREQUENCY_HZ >= 400000U) {
        ccr = rcc_apb_frequency / (3U * I2C_BUS_FREQUENCY_HZ);
        if (ccr == 0U) {
            ccr = 1U;
        }
        I2C_CCR_REG = I2C_CCR_FS_BIT | ccr;
        I2C_TRISE_REG = ((peripheral_clock_mhz * 300U) / 1000U) + 1U;
    } else {
        ccr = rcc_apb_frequency / (2U * I2C_BUS_FREQUENCY_HZ);
        if (ccr < 4U) {
            ccr = 4U;
        }
        I2C_CCR_REG = ccr;
        I2C_TRISE_REG = peripheral_clock_mhz + 1U;
    }

    I2C_CR1_REG = I2C_CR1_PE_BIT | I2C_CR1_ACK_BIT;
    I2C_CR2_REG = peripheral_clock_mhz | I2C_CR2_ITERREN_BIT |
                  I2C_CR2_ITEVTEN_BIT | I2C_CR2_ITBUFEN_BIT;
}

static void i2c_direct_recover(void)
{
    I2C_CR1_REG = I2C_CR1_SWRST_BIT;
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    I2C_CR1_REG = 0U;

    i2c_write_length = 0U;
    i2c_write_overflow = 0U;
    i2c_read_index = 0U;
    i2c_direct_configure();
}

void i2c1_isr(void)
{
    uint32_t status = I2C_SR1_REG;

    if ((status & I2C_ERROR_BITS) != 0U) {
        I2C_SR1_REG &= ~(status & I2C_ERROR_BITS);
        i2c_direct_recover();
        return;
    }

    /* Clear the previous read NACK before handling a new address or TXE. */
    if ((status & I2C_SR1_AF_BIT) != 0U) {
        I2C_SR1_REG &= ~I2C_SR1_AF_BIT;
        i2c_read_length = 0U;
        i2c_read_index = 0U;
        return;
    }

    if ((status & I2C_SR1_ADDR_BIT) != 0U) {
        uint32_t direction = I2C_SR2_REG; /* SR1 then SR2 clears ADDR. */

        if ((direction & I2C_SR2_TRA_BIT) != 0U) {
            i2c_read_index = 0U;
            I2C_DR_REG = (i2c_read_length != 0U)
                             ? i2c_read_buffer[i2c_read_index++]
                             : 0xFFU;
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
    }

    if ((status & I2C_SR1_STOPF_BIT) != 0U) {
        (void)I2C_SR1_REG;
        I2C_CR1_REG |= I2C_CR1_PE_BIT | I2C_CR1_ACK_BIT;
        if (i2c_write_length != 0U && i2c_write_overflow == 0U) {
            i2c_write_ready = 1U;
        }
        return;
    }

    /* TXE must always be serviced or the slave can remain in an IRQ loop. */
    if ((status & I2C_SR1_TXE_BIT) != 0U) {
        I2C_DR_REG = (i2c_read_index < i2c_read_length)
                         ? i2c_read_buffer[i2c_read_index++]
                         : 0xFFU;
    }
}

static void i2c_slave_setup(void)
{
#ifdef I2C_SLAVE_PF0_PF1
    rcc_periph_clock_enable(RCC_GPIOF);
    rcc_periph_clock_enable(RCC_I2C1);

    /* LockNode wiring: PF1=SCL, PF0=SDA, both on AF12. */
    gpio_mode_setup(GPIOF, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO0 | GPIO1);
    gpio_set_output_options(GPIOF, GPIO_OTYPE_OD, GPIO_OSPEED_HIGH, GPIO0 | GPIO1);
    gpio_set_af(GPIOF, GPIO_AF12, GPIO0 | GPIO1);
#else
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_SYSCFG);
    rcc_periph_clock_enable(RCC_I2C1);

    /* Configure only PB6=SCL and PA10=SDA. Leave PA13/SWDIO and PA14/SWCLK
     * untouched so the programmed firmware does not disable SWD. */
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO6);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_HIGH, GPIO6);
    gpio_set_af(GPIOB, GPIO_AF6, GPIO6);

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO10);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_HIGH, GPIO10);
    gpio_set_af(GPIOA, GPIO_AF6, GPIO10);

    SYSCFG_CFGR1_REG |= SYSCFG_I2C_PA10_ANF | SYSCFG_I2C_PB6_ANF;
#endif

    i2c_direct_recover();

    nvic_set_priority(NVIC_I2C1_IRQ, 0U);
    nvic_enable_irq(NVIC_I2C1_IRQ);
}

#endif
