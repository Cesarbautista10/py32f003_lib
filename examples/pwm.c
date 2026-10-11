#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/memorymap.h>
#include <libopencm3/cm3/systick.h>

#define PWM_PORT              GPIOB
#define PWM_PIN               GPIO5
#define PWM_FREQUENCY_HZ      1000U
#define PWM_PERIOD_TICKS      1000U
#define PWM_STEP_DELAY_MS     20U

#define TIM3_CR1              MMIO32(TIM3_BASE + 0x00U)
#define TIM3_EGR              MMIO32(TIM3_BASE + 0x14U)
#define TIM3_CCMR1            MMIO32(TIM3_BASE + 0x18U)
#define TIM3_CCER             MMIO32(TIM3_BASE + 0x20U)
#define TIM3_PSC              MMIO32(TIM3_BASE + 0x28U)
#define TIM3_ARR              MMIO32(TIM3_BASE + 0x2CU)
#define TIM3_CCR2             MMIO32(TIM3_BASE + 0x38U)

#define TIM_CR1_CEN           (1U << 0)
#define TIM_CR1_ARPE          (1U << 7)
#define TIM_EGR_UG            (1U << 0)
#define TIM_CCMR1_OC2PE       (1U << 11)
#define TIM_CCMR1_OC2M_PWM1   (6U << 12)
#define TIM_CCER_CC2E         (1U << 4)

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

static void pwm_setup(void)
{
    uint32_t prescaler;

    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_TIM3);

    /* PB5 is TIM3_CH2 on alternate function 1. */
    gpio_mode_setup(PWM_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, PWM_PIN);
    gpio_set_output_options(PWM_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, PWM_PIN);
    gpio_set_af(PWM_PORT, GPIO_AF1, PWM_PIN);

    /* 8 MHz / 8 / 1000 = 1 kHz PWM. */
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

static void pwm_set_duty_per_mille(uint32_t duty)
{
    if (duty > 1000U) {
        duty = 1000U;
    }

    TIM3_CCR2 = duty;
}

int main(void)
{
    uint32_t duty;

    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_8MHZ);
    systick_setup();
    pwm_setup();

    while (1) {
        for (duty = 0U; duty <= 1000U; duty += 10U) {
            pwm_set_duty_per_mille(duty);
            delay_ms(PWM_STEP_DELAY_MS);
        }

        for (duty = 1000U; duty > 0U; duty -= 10U) {
            pwm_set_duty_per_mille(duty);
            delay_ms(PWM_STEP_DELAY_MS);
        }
    }
}
