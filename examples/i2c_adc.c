#include <stdint.h>

#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/memorymap.h>

/* Change this 7-bit address when another device already uses 0x20. */
#define I2C_SLAVE_ADDRESS  0x20U
#define REG_DEVICE_TYPE    0x00U
#define REG_ADC_RAW        0x01U
#define DEVICE_TYPE_ADC    0x01U
/* 0x01 = TEMT6000. Use 0x00 for another generic analog sensor. */
#define DEVICE_MODEL       0x01U

#define ADC_INPUT_PORT     GPIOA
#define ADC_INPUT_PIN      GPIO2
#define ADC_CHANNEL        2U
#define ADC_MAX_VALUE      4095U
#define ADC_TIMEOUT_CYCLES 100000U

#define ADC_ISR            MMIO32(ADC1_BASE + 0x00U)
#define ADC_CR             MMIO32(ADC1_BASE + 0x08U)
#define ADC_CFGR1          MMIO32(ADC1_BASE + 0x0CU)
#define ADC_CFGR2          MMIO32(ADC1_BASE + 0x10U)
#define ADC_SMPR           MMIO32(ADC1_BASE + 0x14U)
#define ADC_CHSELR         MMIO32(ADC1_BASE + 0x28U)
#define ADC_DR             MMIO32(ADC1_BASE + 0x40U)
#define ADC_ISR_EOC        (1U << 2)
#define ADC_ISR_EOSEQ      (1U << 3)
#define ADC_ISR_OVR        (1U << 4)
#define ADC_CR_ADEN        (1U << 0)
#define ADC_CR_ADSTART     (1U << 2)
#define ADC_CR_ADSTP       (1U << 4)
#define ADC_CR_ADCAL       (1U << 31)
#define ADC_CFGR2_PCLK_DIV2 (1U << 28)
#define ADC_SMPR_239_5_CYCLES 7U

#include "i2c_slave_minimal.h"

static uint8_t adc_available;

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles-- != 0U) {
        __asm__ volatile ("nop");
    }
}

static uint8_t adc_setup(void)
{
    uint32_t timeout;

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_ADC1);
    gpio_mode_setup(ADC_INPUT_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                    ADC_INPUT_PIN);

    /* Configure the ADC clock while the peripheral is still disabled. */
    ADC_CFGR2 = ADC_CFGR2_PCLK_DIV2;
    ADC_CFGR1 = 0U;

    /* Calibration must run once while the ADC is disabled. */
    ADC_CR |= ADC_CR_ADCAL;
    timeout = ADC_TIMEOUT_CYCLES;
    while (((ADC_CR & ADC_CR_ADCAL) != 0U) && (timeout != 0U)) {
        timeout--;
    }
    if (timeout == 0U) {
        return 0U;
    }

    return 1U;
}

static void adc_stop(void)
{
    uint32_t timeout = ADC_TIMEOUT_CYCLES;

    if ((ADC_CR & ADC_CR_ADSTART) != 0U) {
        ADC_CR |= ADC_CR_ADSTP;
        while (((ADC_CR & ADC_CR_ADSTART) != 0U) && (timeout != 0U)) {
            timeout--;
        }
    }

    ADC_CR &= ~ADC_CR_ADEN;
    timeout = ADC_TIMEOUT_CYCLES;
    while (((ADC_CR & ADC_CR_ADEN) != 0U) && (timeout != 0U)) {
        timeout--;
    }
}

static uint8_t adc_convert_single(uint16_t *value)
{
    uint32_t timeout = ADC_TIMEOUT_CYCLES;

    /* Same sequence as HAL_ADC_Start(): enable, stabilize, clear, start. */
    ADC_CR |= ADC_CR_ADEN;
    delay_cycles(8U);
    ADC_ISR = ADC_ISR_EOC | ADC_ISR_EOSEQ | ADC_ISR_OVR;
    ADC_CR |= ADC_CR_ADSTART;
    while (((ADC_ISR & ADC_ISR_EOC) == 0U) && (timeout != 0U)) {
        timeout--;
    }
    if (timeout == 0U) {
        adc_stop();
        return 0U;
    }

    *value = (uint16_t)(ADC_DR & ADC_MAX_VALUE);
    /* TEMT6000 reference calls HAL_ADC_Stop() after every conversion. */
    adc_stop();
    return 1U;
}

static uint8_t adc_read_stable(uint16_t *value)
{
    uint16_t discarded;

    /* The reference reselects PA2 and the long sampling time on every read. */
    adc_stop();
    ADC_CHSELR = 1U << ADC_CHANNEL;
    ADC_SMPR = ADC_SMPR_239_5_CYCLES;

    /* Discard the first sample after selecting/enabling the ADC channel. */
    if (adc_convert_single(&discarded) == 0U) {
        return 0U;
    }
    return adc_convert_single(value);
}

static void process_i2c_write(const uint8_t *data, uint8_t length)
{
    uint16_t value;

    (void)length;

    switch (data[0]) {
    case REG_DEVICE_TYPE:
        /* Byte 0: node type. Byte 1: attached sensor model. */
        i2c_set_response_u16_le(DEVICE_TYPE_ADC |
                                (DEVICE_MODEL << 8));
        break;

    case REG_ADC_RAW:
        /* Response order: low byte first, then high byte. */
        if ((adc_available != 0U) && (adc_read_stable(&value) != 0U)) {
            i2c_set_response_u16_le(value);
        } else {
            adc_available = 0U;
            i2c_set_response_u16_le(0xFFFFU);
        }
        break;

    default:
        i2c_set_response_u8(0xFFU);
        break;
    }
}

int main(void)
{
    uint8_t request[I2C_WRITE_BUFFER_SIZE];
    uint8_t length;
    uint16_t value;

    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_8MHZ);

    /* Bring up I2C first so an ADC fault cannot prevent bus communication. */
    i2c_set_response_u16_le(0xFFFFU);
    i2c_slave_setup();
    adc_available = adc_setup();
    if ((adc_available != 0U) && (adc_read_stable(&value) != 0U)) {
        i2c_set_response_u16_le(value);
    } else {
        adc_available = 0U;
    }

    while (1) {
        length = i2c_take_write(request, sizeof(request));
        if (length != 0U) {
            process_i2c_write(request, length);
        }
    }
}
