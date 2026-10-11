#include <stdint.h>

#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/memorymap.h>

/* Change this 7-bit address when another device already uses 0x20. */
#define I2C_SLAVE_ADDRESS  0x20U
#define REG_ADC_RAW        0x01U

#define ADC_INPUT_PORT     GPIOA
#define ADC_INPUT_PIN      GPIO2
#define ADC_CHANNEL        2U
#define ADC_MAX_VALUE      4095U

#define ADC_ISR            MMIO32(ADC1_BASE + 0x00U)
#define ADC_CR             MMIO32(ADC1_BASE + 0x08U)
#define ADC_CFGR1          MMIO32(ADC1_BASE + 0x0CU)
#define ADC_CFGR2          MMIO32(ADC1_BASE + 0x10U)
#define ADC_SMPR           MMIO32(ADC1_BASE + 0x14U)
#define ADC_CHSELR         MMIO32(ADC1_BASE + 0x28U)
#define ADC_DR             MMIO32(ADC1_BASE + 0x40U)
#define ADC_CCSR           MMIO32(ADC1_BASE + 0x44U)
#define ADC_CAL_RESULT1    MMIO32(ADC1_BASE + 0x48U)
#define ADC_CAL_RESULT2    MMIO32(ADC1_BASE + 0x4CU)
#define ADC_CAL_FACTOR1    MMIO32(ADC1_BASE + 0x50U)
#define ADC_CAL_FACTOR2    MMIO32(ADC1_BASE + 0x54U)

#define ADC_ISR_EOC        (1U << 2)
#define ADC_CR_ADEN        (1U << 0)
#define ADC_CR_ADSTART     (1U << 2)
#define ADC_CR_ADCAL       (1U << 31)
#define ADC_CFGR1_OVRMOD   (1U << 12)
#define ADC_CFGR2_PCLK_DIV4 (2U << 28)
#define ADC_SMPR_41_5_CYCLES 5U
#define ADC_CCSR_CAL_ENABLE (1U << 15)

#include "i2c_slave_minimal.h"

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles-- != 0U) {
        __asm__ volatile ("nop");
    }
}

static void sort_calibration_samples(int32_t samples[5])
{
    uint32_t i;

    for (i = 1U; i < 5U; i++) {
        int32_t value = samples[i];
        uint32_t position = i;

        while (position > 0U && samples[position - 1U] > value) {
            samples[position] = samples[position - 1U];
            position--;
        }
        samples[position] = value;
    }
}

static void adc_setup(void)
{
    int32_t samples[5];
    uint32_t i;

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_ADC1);
    gpio_mode_setup(ADC_INPUT_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                    ADC_INPUT_PIN);

    /* Puya recommends five calibrations and the median result. */
    for (i = 0U; i < 5U; i++) {
        ADC_CR = ADC_CR_ADCAL;
        while ((ADC_CR & ADC_CR_ADCAL) != 0U) {
        }
        samples[i] = (int32_t)(ADC_CAL_RESULT1 << 9);
    }
    sort_calibration_samples(samples);
    ADC_CAL_FACTOR1 = (uint32_t)(samples[2] >> 9);
    ADC_CAL_FACTOR2 = ADC_CAL_RESULT2;
    ADC_CCSR |= ADC_CCSR_CAL_ENABLE;

    ADC_CFGR2 = ADC_CFGR2_PCLK_DIV4;
    ADC_CFGR1 = ADC_CFGR1_OVRMOD;
    ADC_SMPR = ADC_SMPR_41_5_CYCLES;
    ADC_CHSELR = 1U << ADC_CHANNEL;
    delay_cycles(8000U);
    ADC_CR = ADC_CR_ADEN;
    delay_cycles(8000U);
}

static uint16_t adc_read(void)
{
    ADC_ISR = ADC_ISR_EOC;
    ADC_CR = ADC_CR_ADSTART;
    while ((ADC_ISR & ADC_ISR_EOC) == 0U) {
    }
    ADC_ISR = ADC_ISR_EOC;
    return (uint16_t)(ADC_DR & ADC_MAX_VALUE);
}

static void process_i2c_write(const uint8_t *data, uint8_t length)
{
    (void)length;

    switch (data[0]) {
    case REG_ADC_RAW:
        /* Response order: low byte first, then high byte. */
        i2c_set_response_u16_le(adc_read());
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

    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_8MHZ);
    adc_setup();
    i2c_set_response_u16_le(adc_read());
    i2c_slave_setup();

    while (1) {
        length = i2c_take_write(request, sizeof(request));
        if (length != 0U) {
            process_i2c_write(request, length);
        }
    }
}
