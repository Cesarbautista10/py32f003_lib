#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/memorymap.h>
#include <libopencm3/cm3/systick.h>

#define PWM_PORT              GPIOA
#define PWM_PIN               GPIO0
#define PWM_FREQUENCY_HZ      1000U
#define PWM_PERIOD_TICKS      1000U
#define PWM_STEP_DELAY_MS     20U

#define TIM1_CR1              MMIO32(TIM1_BASE + 0x00U)
#define TIM1_EGR              MMIO32(TIM1_BASE + 0x14U)
#define TIM1_CCMR2            MMIO32(TIM1_BASE + 0x1CU)
#define TIM1_CCER             MMIO32(TIM1_BASE + 0x20U)
#define TIM1_PSC              MMIO32(TIM1_BASE + 0x28U)
#define TIM1_ARR              MMIO32(TIM1_BASE + 0x2CU)
#define TIM1_CCR3             MMIO32(TIM1_BASE + 0x3CU)
#define TIM1_BDTR             MMIO32(TIM1_BASE + 0x44U)

#define TIM_CR1_CEN           (1U << 0)
#define TIM_CR1_ARPE          (1U << 7)
#define TIM_EGR_UG            (1U << 0)
#define TIM_CCMR2_OC3PE       (1U << 3)
#define TIM_CCMR2_OC3M_PWM1   (6U << 4)
#define TIM_CCER_CC3E         (1U << 8)
#define TIM_BDTR_MOE          (1U << 15)

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

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_TIM1);

    /* PA0 is TIM1_CH3 on alternate function 13. */
    gpio_mode_setup(PWM_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, PWM_PIN);
    gpio_set_output_options(PWM_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, PWM_PIN);
    gpio_set_af(PWM_PORT, GPIO_AF13, PWM_PIN);

    /* 8 MHz / 8 / 1000 = 1 kHz PWM. */
    prescaler = rcc_apb_frequency / (PWM_FREQUENCY_HZ * PWM_PERIOD_TICKS);

    TIM1_CR1 = 0U;
    TIM1_PSC = prescaler - 1U;
    TIM1_ARR = PWM_PERIOD_TICKS - 1U;
    TIM1_CCR3 = 0U;
    TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
    TIM1_CCER = TIM_CCER_CC3E;
    TIM1_BDTR = TIM_BDTR_MOE;
    TIM1_EGR = TIM_EGR_UG;
    TIM1_CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static void pwm_set_duty_per_mille(uint32_t duty)
{
    if (duty > 1000U) {
        duty = 1000U;
    }

    TIM1_CCR3 = duty;
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
