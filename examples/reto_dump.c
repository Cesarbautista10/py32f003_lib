#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/usart.h>

#define LED_PORT GPIOB
#define LED_PIN GPIO5

#define UART_PORT GPIOA
#define UART_TX GPIO0
#define UART_RX GPIO1
#define UART_DEV USART2

#define RX_BUFFER_SIZE 40U
#define ACCESS_CODE 261126U
#define FLAG_XOR_KEY 0x5aU

/*
 * Firmware didactico para el taller "Del header al dump".
 *
 * Las cadenas visibles orientan el reconocimiento inicial con `strings`. El
 * codigo de acceso y la bandera codificada obligan a continuar con el
 * desensamblado. No se trata de un mecanismo de seguridad para produccion.
 */
static const uint8_t encoded_flag[] = {
    0x18, 0x0f, 0x1d, 0x19, 0x15, 0x14, 0x21, 0x09, 0x0d,
    0x1e, 0x05, 0x1e, 0x0f, 0x17, 0x0a, 0x05, 0x0c, 0x1f,
    0x08, 0x13, 0x1c, 0x13, 0x1f, 0x1e, 0x27,
};

/* The leading NUL keeps `strings` from joining this clue to code bytes. */
static const char auth_ok_text[] = "\0AUTH OK: ";
static const char auth_denied_text[] = "\0AUTH DENIED\r\n";

static void delay(volatile uint32_t cycles)
{
    while (cycles-- != 0U) {
        __asm__ volatile ("nop");
    }
}

static void uart_send_char(char ch)
{
    usart_send_blocking(UART_DEV, (uint16_t)ch);
}

static void uart_send_string(const char *text)
{
    while (*text != '\0') {
        uart_send_char(*text++);
    }
}

static bool strings_equal(const char *left, const char *right)
{
    while ((*left != '\0') && (*left == *right)) {
        left++;
        right++;
    }

    return *left == *right;
}

static bool starts_with(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return false;
        }
    }

    return true;
}

static bool parse_decimal(const char *text, uint32_t *value)
{
    uint32_t result = 0U;
    bool has_digits = false;

    while (*text != '\0') {
        uint32_t digit;

        if ((*text < '0') || (*text > '9')) {
            return false;
        }

        digit = (uint32_t)(*text - '0');
        if (result > ((UINT32_MAX - digit) / 10U)) {
            return false;
        }

        result = (result * 10U) + digit;
        has_digits = true;
        text++;
    }

    *value = result;
    return has_digits;
}

__attribute__((noinline))
static void reveal_flag(void)
{
    size_t index;

    uart_send_string(&auth_ok_text[1]);
    for (index = 0U; index < sizeof(encoded_flag); index++) {
        uart_send_char((char)(encoded_flag[index] ^ FLAG_XOR_KEY));
    }
    uart_send_string("\r\n");
}

__attribute__((noinline))
static void authenticate(const char *argument)
{
    uint32_t candidate = 0U;

    if (parse_decimal(argument, &candidate) && (candidate == ACCESS_CODE)) {
        reveal_flag();
        gpio_set(LED_PORT, LED_PIN);
    } else {
        uart_send_string(&auth_denied_text[1]);
        gpio_clear(LED_PORT, LED_PIN);
    }
}

__attribute__((noinline))
static void handle_command(const char *command)
{
    if (strings_equal(command, "help")) {
        uart_send_string("help | status | led on | led off | auth <codigo>\r\n");
    } else if (strings_equal(command, "status")) {
        uart_send_string("MODEL=PY32F003x4; DEBUG=OPEN; AUTH=LOCKED\r\n");
    } else if (strings_equal(command, "led on")) {
        gpio_set(LED_PORT, LED_PIN);
        uart_send_string("LED ON\r\n");
    } else if (strings_equal(command, "led off")) {
        gpio_clear(LED_PORT, LED_PIN);
        uart_send_string("LED OFF\r\n");
    } else if (starts_with(command, "auth ")) {
        authenticate(command + 5);
    } else if (*command != '\0') {
        uart_send_string("ERR: usa help\r\n");
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
    gpio_set_output_options(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, LED_PIN);
    gpio_clear(LED_PORT, LED_PIN);

    gpio_mode_setup(UART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART_TX | UART_RX);
    gpio_set_output_options(UART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, UART_TX);
    gpio_set_af(UART_PORT, GPIO_AF9, UART_TX | UART_RX);
}

static void usart_setup(void)
{
    usart_disable(UART_DEV);
    usart_set_baudrate(UART_DEV, 115200U);
    usart_set_databits(UART_DEV, 8U);
    usart_set_stopbits(UART_DEV, USART_CR2_STOPBITS_1);
    usart_set_parity(UART_DEV, USART_PARITY_NONE);
    usart_set_mode(UART_DEV, USART_MODE_TX_RX);
    usart_set_flow_control(UART_DEV, USART_FLOWCONTROL_NONE);
    usart_enable(UART_DEV);
}

int main(void)
{
    char command[RX_BUFFER_SIZE];
    size_t length = 0U;

    clock_setup();
    gpio_setup();
    usart_setup();
    delay(2400000U);

    uart_send_string("\r\nBUGCON 2026 / DUMP-LAB v1.0\r\n");
    uart_send_string("Escribe help y presiona Enter.\r\n> ");

    while (true) {
        uint16_t received;

        if (usart_get_flag(UART_DEV, USART_SR_FE) ||
            usart_get_flag(UART_DEV, USART_SR_NE) ||
            usart_get_flag(UART_DEV, USART_SR_ORE)) {
            (void)usart_recv(UART_DEV);
            continue;
        }

        if (!usart_get_flag(UART_DEV, USART_SR_RXNE)) {
            continue;
        }

        received = usart_recv(UART_DEV);
        if ((received == '\r') || (received == '\n')) {
            if (length != 0U) {
                command[length] = '\0';
                uart_send_string("\r\n");
                handle_command(command);
                length = 0U;
                uart_send_string("> ");
            }
        } else if ((received == '\b') || (received == 0x7fU)) {
            if (length != 0U) {
                length--;
                uart_send_string("\b \b");
            }
        } else if ((received >= 32U) && (received <= 126U) &&
                   (length < (RX_BUFFER_SIZE - 1U))) {
            command[length++] = (char)received;
            uart_send_char((char)received);
        }
    }
}
