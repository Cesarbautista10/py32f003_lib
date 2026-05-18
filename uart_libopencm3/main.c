/*
 * UART example for PY32F003 using libopencm3.
 *
 * UART mapping aligned with the HAL/Rust project:
 *   PA0 -> USART2_TX (AF9)
 *   PA1 -> USART2_RX (AF9)
 *
 * Also blinks PB5 so you can confirm firmware is running.
 */

#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/usart.h>

#define LED_PORT  GPIOB
#define LED_PIN   GPIO5

#define UART_PORT GPIOA
#define UART_TX   GPIO0
#define UART_RX   GPIO1
#define UART_DEV  USART2

static void delay(volatile uint32_t cycles)
{
	while (cycles--) {
		__asm__ volatile ("nop");
	}
}

static void uart_send_string(const char *s)
{
	while (*s) {
		usart_send_blocking(UART_DEV, (uint16_t)(*s));
		s++;
	}
}

static void clock_setup(void)
{
	rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_8MHZ);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_USART2);
}

static void gpio_setup(void)
{
	gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
	gpio_set_output_options(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LOW,
				LED_PIN);

	gpio_mode_setup(UART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
			UART_TX | UART_RX);
	gpio_set_output_options(UART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH,
				UART_TX);
	gpio_set_af(UART_PORT, GPIO_AF9, UART_TX | UART_RX);
}

static void usart_setup(void)
{
	usart_disable(UART_DEV);
	usart_set_baudrate(UART_DEV, 115200);
	usart_set_databits(UART_DEV, 8);
	usart_set_stopbits(UART_DEV, USART_CR2_STOPBITS_1);
	usart_set_parity(UART_DEV, USART_PARITY_NONE);
	usart_set_mode(UART_DEV, USART_MODE_TX_RX);
	usart_set_flow_control(UART_DEV, USART_FLOWCONTROL_NONE);
	usart_enable(UART_DEV);
}

int main(void)
{
	clock_setup();
	gpio_setup();
	usart_setup();

	uart_send_string("PY32F003 libopencm3 ECHO OK\r\n");
	uart_send_string("Escribe y te hago echo:\r\n");

	while (1) {
		/* RX line may pick up noise. Drop error frames and only echo
		 * printable ASCII (plus CR/LF) to keep terminal output clean. */
		if (usart_get_flag(UART_DEV, USART_SR_FE) ||
		    usart_get_flag(UART_DEV, USART_SR_NE) ||
		    usart_get_flag(UART_DEV, USART_SR_ORE)) {
			(void)usart_recv(UART_DEV);
		}

		if (usart_get_flag(UART_DEV, USART_SR_RXNE)) {
			uint16_t ch = usart_recv(UART_DEV);
			if (ch == '\r') {
				usart_send_blocking(UART_DEV, '\r');
				usart_send_blocking(UART_DEV, '\n');
				gpio_toggle(LED_PORT, LED_PIN);
			} else if ((ch >= 32U && ch <= 126U) || ch == '\n') {
				usart_send_blocking(UART_DEV, ch);
				gpio_toggle(LED_PORT, LED_PIN);
			}
		}

		delay(2000);
	}
}
