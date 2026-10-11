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
#define ADC_TIMEOUT_MS        2U
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
#define ADC_ISR_EOC           (1U << 2)
#define ADC_ISR_EOSEQ         (1U << 3)
#define ADC_ISR_OVR           (1U << 4)
#define ADC_CR_ADEN           (1U << 0)
#define ADC_CR_ADSTART        (1U << 2)
#define ADC_CR_ADSTP          (1U << 4)
#define ADC_CR_ADCAL          (1U << 31)
#define ADC_CFGR2_PCLK_DIV2   (1U << 28)
#define ADC_SMPR_239_5_CYCLES 7U

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

static uint8_t adc_calibrate(void)
{
    uint32_t start;

    /* Calibration must run once while the ADC is disabled. */
    ADC_CR |= ADC_CR_ADCAL;
    start = system_millis;
    while ((ADC_CR & ADC_CR_ADCAL) != 0U) {
        if ((uint32_t)(system_millis - start) > ADC_TIMEOUT_MS) {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t adc_setup(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_ADC1);

    gpio_mode_setup(ADC_INPUT_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                    ADC_INPUT_PIN);

    /* Match TEMT6000: PCLK/2, 12-bit, right aligned, single conversion. */
    ADC_CFGR2 = ADC_CFGR2_PCLK_DIV2;
    ADC_CFGR1 = 0U;
    return adc_calibrate();
}

static uint8_t adc_stop(void)
{
    uint32_t start;

    if ((ADC_CR & ADC_CR_ADSTART) != 0U) {
        ADC_CR |= ADC_CR_ADSTP;
        start = system_millis;
        while ((ADC_CR & ADC_CR_ADSTART) != 0U) {
            if ((uint32_t)(system_millis - start) > ADC_TIMEOUT_MS) {
                return 0U;
            }
        }
    }

    ADC_CR &= ~ADC_CR_ADEN;
    start = system_millis;
    while ((ADC_CR & ADC_CR_ADEN) != 0U) {
        if ((uint32_t)(system_millis - start) > ADC_TIMEOUT_MS) {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t adc_convert_single(uint16_t *value)
{
    uint32_t start;

    ADC_CR |= ADC_CR_ADEN;
    delay_ms(1U);
    ADC_ISR = ADC_ISR_EOC | ADC_ISR_EOSEQ | ADC_ISR_OVR;
    ADC_CR |= ADC_CR_ADSTART;

    start = system_millis;
    while ((ADC_ISR & ADC_ISR_EOC) == 0U) {
        if ((uint32_t)(system_millis - start) > ADC_TIMEOUT_MS) {
            (void)adc_stop();
            return 0U;
        }
    }

    *value = (uint16_t)(ADC_DR & ADC_MAX_VALUE);
    return adc_stop();
}

static uint8_t adc_read_stable(uint16_t *value)
{
    uint16_t discarded;

    (void)adc_stop();
    ADC_CHSELR = 1U << ADC_CHANNEL;
    ADC_SMPR = ADC_SMPR_239_5_CYCLES;

    /* Discard the first sample after selecting/enabling the ADC channel. */
    if (adc_convert_single(&discarded) == 0U) {
        return 0U;
    }
    return adc_convert_single(value);
}

int main(void)
{
    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_8MHZ);
    systick_setup();
    uart_setup();
    uart_send_string("\r\nPY32F003 ADC: PA2 / ADC_IN2\r\n");
    if (adc_setup() == 0U) {
        uart_send_string("ADC initialization error\r\n");
    }

    while (1) {
        uint16_t raw;

        if (adc_read_stable(&raw) != 0U) {
            uint32_t millivolts =
                (((uint32_t)raw * ADC_REFERENCE_MV) +
                 (ADC_MAX_VALUE / 2U)) / ADC_MAX_VALUE;

            uart_send_string("ADC=");
            uart_send_u32(raw);
            uart_send_string("/4095, voltage=");
            uart_send_u32(millivolts);
            uart_send_string(" mV\r\n");
        } else {
            uart_send_string("ADC read error\r\n");
        }

        delay_ms(SAMPLE_INTERVAL_MS);
    }
}
