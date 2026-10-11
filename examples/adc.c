#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/usart.h>
#include <libopencm3/py32/memorymap.h>
#include <libopencm3/cm3/systick.h>

#define ADC_INPUT_PORT        GPIOA
#define ADC_INPUT_PIN         GPIO2
#define ADC_CHANNEL           2U
#define ADC_REFERENCE_MV      3300U
#define ADC_MAX_VALUE         4095U
#define SAMPLE_INTERVAL_MS    250U

#define UART_PORT             GPIOA
#define UART_TX_PIN           GPIO0
#define UART_DEV              USART2

#define ADC_ISR               MMIO32(ADC1_BASE + 0x00U)
#define ADC_CR                MMIO32(ADC1_BASE + 0x08U)
#define ADC_CFGR1             MMIO32(ADC1_BASE + 0x0CU)
#define ADC_CFGR2             MMIO32(ADC1_BASE + 0x10U)
#define ADC_SMPR              MMIO32(ADC1_BASE + 0x14U)
#define ADC_CHSELR            MMIO32(ADC1_BASE + 0x28U)
#define ADC_DR                MMIO32(ADC1_BASE + 0x40U)
#define ADC_CCSR              MMIO32(ADC1_BASE + 0x44U)
#define ADC_CAL_RESULT1       MMIO32(ADC1_BASE + 0x48U)
#define ADC_CAL_RESULT2       MMIO32(ADC1_BASE + 0x4CU)
#define ADC_CAL_FACTOR1       MMIO32(ADC1_BASE + 0x50U)
#define ADC_CAL_FACTOR2       MMIO32(ADC1_BASE + 0x54U)

#define ADC_ISR_EOC           (1U << 2)
#define ADC_CR_ADEN           (1U << 0)
#define ADC_CR_ADSTART        (1U << 2)
#define ADC_CR_ADCAL          (1U << 31)
#define ADC_CFGR1_OVRMOD      (1U << 12)
#define ADC_CFGR2_PCLK_DIV4   (2U << 28)
#define ADC_SMPR_41_5_CYCLES  5U
#define ADC_CCSR_CAL_ENABLE   (1U << 15)

static volatile uint32_t system_millis;

void sys_tick_handler(void)
{
    system_millis++;
}

static void delay_ms(uint32_t duration_ms)
{
    uint32_t start = system_millis;

    while ((uint32_t)(system_millis - start) < duration_ms) {
        /* Wait for the 1 ms SysTick interrupt. */
    }
}

static void systick_setup(void)
{
    systick_set_frequency(1000U, rcc_ahb_frequency);
    systick_clear();
    systick_interrupt_enable();
    systick_counter_enable();
}

static void uart_send_string(const char *text)
{
    while (*text != '\0') {
        usart_send_blocking(UART_DEV, (uint16_t)*text);
        text++;
    }
}

static void uart_send_u32(uint32_t value)
{
    char digits[10];
    uint32_t count = 0U;

    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);

    while (count > 0U) {
        usart_send_blocking(UART_DEV, (uint16_t)digits[--count]);
    }
}

static void uart_setup(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);

    gpio_mode_setup(UART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART_TX_PIN);
    gpio_set_output_options(UART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH,
                            UART_TX_PIN);
    gpio_set_af(UART_PORT, GPIO_AF9, UART_TX_PIN);

    usart_disable(UART_DEV);
    usart_set_baudrate(UART_DEV, 115200U);
    usart_set_databits(UART_DEV, 8U);
    usart_set_stopbits(UART_DEV, USART_CR2_STOPBITS_1);
    usart_set_parity(UART_DEV, USART_PARITY_NONE);
    usart_set_mode(UART_DEV, USART_MODE_TX);
    usart_set_flow_control(UART_DEV, USART_FLOWCONTROL_NONE);
    usart_enable(UART_DEV);
}

static void sort_calibration_samples(int32_t samples[5])
{
    uint32_t i;

    for (i = 1U; i < 5U; i++) {
        int32_t value = samples[i];
        uint32_t position = i;

        while ((position > 0U) && (samples[position - 1U] > value)) {
            samples[position] = samples[position - 1U];
            position--;
        }
        samples[position] = value;
    }
}

static void adc_calibrate(void)
{
    int32_t samples[5];
    uint32_t i;

    /* Puya recommends five calibrations and applying the median result. */
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
}

static void adc_setup(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_ADC1);

    gpio_mode_setup(ADC_INPUT_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                    ADC_INPUT_PIN);

    adc_calibrate();

    /* ADC clock = 8 MHz / 4, 12-bit right aligned, single conversion. */
    ADC_CFGR2 = ADC_CFGR2_PCLK_DIV4;
    ADC_CFGR1 = ADC_CFGR1_OVRMOD;
    ADC_SMPR = ADC_SMPR_41_5_CYCLES;
    ADC_CHSELR = (1U << ADC_CHANNEL);

    delay_ms(1U);
    ADC_CR = ADC_CR_ADEN;
    delay_ms(1U);
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

int main(void)
{
    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_8MHZ);
    systick_setup();
    uart_setup();
    adc_setup();

    uart_send_string("\r\nPY32F003 ADC: PA2 / ADC_IN2\r\n");

    while (1) {
        uint32_t raw = adc_read();
        uint32_t millivolts =
            ((raw * ADC_REFERENCE_MV) + (ADC_MAX_VALUE / 2U)) / ADC_MAX_VALUE;

        uart_send_string("ADC=");
        uart_send_u32(raw);
        uart_send_string("/4095, voltage=");
        uart_send_u32(millivolts);
        uart_send_string(" mV\r\n");

        delay_ms(SAMPLE_INTERVAL_MS);
    }
}
