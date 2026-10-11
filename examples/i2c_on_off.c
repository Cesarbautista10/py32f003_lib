#include <stdint.h>

#include <libopencm3/py32/rcc.h>
#include <libopencm3/py32/gpio.h>

/* Change this 7-bit address when another device already uses 0x20. */
#define I2C_SLAVE_ADDRESS  0x20U
#define REG_DEVICE_TYPE    0x00U
#define REG_OUTPUT         0x02U
#define DEVICE_TYPE_ON_OFF 0x02U
#define DEVICE_MODEL       0x00U

#define OUTPUT_PORT        GPIOB
#define OUTPUT_PIN         GPIO5

#include "i2c_slave_minimal.h"

static uint8_t output_state;

static void output_setup(void)
{
    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_mode_setup(OUTPUT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, OUTPUT_PIN);
    gpio_set_output_options(OUTPUT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LOW,
                            OUTPUT_PIN);
    gpio_clear(OUTPUT_PORT, OUTPUT_PIN);
}

static void output_write(uint8_t state)
{
    output_state = state != 0U ? 1U : 0U;
    if (output_state != 0U) {
        gpio_set(OUTPUT_PORT, OUTPUT_PIN);
    } else {
        gpio_clear(OUTPUT_PORT, OUTPUT_PIN);
    }
}

static void process_i2c_write(const uint8_t *data, uint8_t length)
{
    switch (data[0]) {
    case REG_DEVICE_TYPE:
        i2c_set_response_u16_le(DEVICE_TYPE_ON_OFF |
                                (DEVICE_MODEL << 8));
        break;

    case REG_OUTPUT:
        if (length >= 2U) {
            output_write(data[1]);
        }
        i2c_set_response_u8(output_state);
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
    output_setup();
    i2c_set_response_u8(output_state);
    i2c_slave_setup();

    while (1) {
        length = i2c_take_write(request, sizeof(request));
        if (length != 0U) {
            process_i2c_write(request, length);
        }
    }
}
