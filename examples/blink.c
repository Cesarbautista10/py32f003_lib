#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/cm3/systick.h>

#define LED_PORT  GPIOB
#define LED_PIN   GPIO5
#define SYSTICK_FREQUENCY_HZ 1000U
#define BLINK_INTERVAL_MS    1000U

static volatile uint32_t system_millis;

void sys_tick_handler(void)
{
    system_millis++;
}

static void systick_setup(void)
{
    systick_set_frequency(SYSTICK_FREQUENCY_HZ, rcc_ahb_frequency);
    systick_clear();
    systick_interrupt_enable();
    systick_counter_enable();
}

static void delay_ms(uint32_t duration_ms)
{
    uint32_t start = system_millis;

    while ((uint32_t)(system_millis - start) < duration_ms) {
        /* Wait for the 1 ms SysTick interrupt. */
    }
}

int main(void)
{
    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_24MHZ);
    systick_setup();
    rcc_periph_clock_enable(RCC_GPIOB);

    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_set_output_options(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, LED_PIN);

    while (1) {
        gpio_toggle(LED_PORT, LED_PIN);
        delay_ms(BLINK_INTERVAL_MS);
    }
}
