#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>

#define LED_PORT  GPIOB
#define LED_PIN   GPIO5

static void delay(volatile uint32_t cycles)
{
    while (cycles--) {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_8MHZ);
    rcc_periph_clock_enable(RCC_GPIOB);

    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_set_output_options(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, LED_PIN);

    while (1) {
        gpio_toggle(LED_PORT, LED_PIN);
        delay(400000);
    }
}
