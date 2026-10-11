#include <stdint.h>

#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/memorymap.h>

/* Change this 7-bit address when another device already uses 0x20. */
#define I2C_SLAVE_ADDRESS  0x20U
#define REG_PWM_PERCENT    0x01U

#define PWM_PORT           GPIOB
#define PWM_PIN            GPIO5
#define PWM_FREQUENCY_HZ   1000U
#define PWM_PERIOD_TICKS   1000U

#define TIM3_CR1           MMIO32(TIM3_BASE + 0x00U)
#define TIM3_EGR           MMIO32(TIM3_BASE + 0x14U)
#define TIM3_CCMR1         MMIO32(TIM3_BASE + 0x18U)
#define TIM3_CCER          MMIO32(TIM3_BASE + 0x20U)
#define TIM3_PSC           MMIO32(TIM3_BASE + 0x28U)
#define TIM3_ARR           MMIO32(TIM3_BASE + 0x2CU)
#define TIM3_CCR2          MMIO32(TIM3_BASE + 0x38U)

#define TIM_CR1_CEN        (1U << 0)
#define TIM_CR1_ARPE       (1U << 7)
#define TIM_EGR_UG         (1U << 0)
#define TIM_CCMR1_OC2PE    (1U << 11)
#define TIM_CCMR1_OC2M_PWM1 (6U << 12)
#define TIM_CCER_CC2E      (1U << 4)

#include "i2c_slave_minimal.h"

static uint8_t pwm_percent;

static void pwm_setup(void)
{
    uint32_t prescaler;

    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_TIM3);

    /* PB5 = TIM3_CH2, alternate function 1. */
    gpio_mode_setup(PWM_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, PWM_PIN);
    gpio_set_output_options(PWM_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, PWM_PIN);
    gpio_set_af(PWM_PORT, GPIO_AF1, PWM_PIN);

    prescaler = rcc_apb_frequency / (PWM_FREQUENCY_HZ * PWM_PERIOD_TICKS);
    TIM3_CR1 = 0U;
    TIM3_PSC = prescaler - 1U;
    TIM3_ARR = PWM_PERIOD_TICKS - 1U;
    TIM3_CCR2 = 0U;
    TIM3_CCMR1 = TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
    TIM3_CCER = TIM_CCER_CC2E;
    TIM3_EGR = TIM_EGR_UG;
    TIM3_CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static void pwm_set_percent(uint8_t percent)
{
    if (percent > 100U) {
        percent = 100U;
    }
    pwm_percent = percent;
    TIM3_CCR2 = ((uint32_t)percent * PWM_PERIOD_TICKS) / 100U;
}

static void process_i2c_write(const uint8_t *data, uint8_t length)
{
    switch (data[0]) {
    case REG_PWM_PERCENT:
        if (length >= 2U) {
            pwm_set_percent(data[1]);
        }
        i2c_set_response_u8(pwm_percent);
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
    pwm_setup();
    pwm_set_percent(0U);
    i2c_set_response_u8(pwm_percent);
    i2c_slave_setup();

    while (1) {
        length = i2c_take_write(request, sizeof(request));
        if (length != 0U) {
            process_i2c_write(request, length);
        }
    }
}
