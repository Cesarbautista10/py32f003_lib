#include <stdint.h>

#include <libopencm3/py32/memorymap.h>
#include <libopencm3/py32/rcc.h>

#include "ws2812_tim1.h"

/* Give this node a unique 7-bit address on the I2C bus. */
#ifndef I2C_SLAVE_ADDRESS
#define I2C_SLAVE_ADDRESS  0x40U
#endif
#ifndef RGB_PA0_I2C_USE_PF0_PF1
#define RGB_PA0_I2C_USE_PF0_PF1 0
#endif

#define I2C_WRITE_BUFFER_SIZE   5U
#if RGB_PA0_I2C_USE_PF0_PF1
#define I2C_SLAVE_PF0_PF1
#endif

/* Shared workshop identification protocol: type, model. */
#define CMD_IDENTIFY            0x00U
#define DEVICE_TYPE_NEOPIXEL    0x04U
#define DEVICE_MODEL_PA0        0x01U

/* LockNode's existing one-byte RGB LED commands. */
#define CMD_RED                 0x02U
#define CMD_GREEN               0x03U
#define CMD_BLUE                0x04U
#define CMD_OFF                 0x05U
#define CMD_WHITE               0x08U

/* Extra commands for direct RGB control. */
#define CMD_FILL_RGB            0x50U /* [command, red, green, blue] */
#define CMD_SET_PIXEL           0x51U /* [command, index, red, green, blue] */
#define CMD_SHOW                0x52U /* [command] */

#define DEFAULT_COLOR_LEVEL     32U
#define RESPONSE_INVALID       0xFFU

#define FLASH_ACR_REG           MMIO32(FLASH_MEM_INTERFACE_BASE + 0x00U)
#define FLASH_ACR_LATENCY_1     (1U << 0)

#include "i2c_slave_minimal.h"

static void process_i2c_write(const uint8_t *data, uint8_t length)
{
    uint8_t response = RESPONSE_INVALID;
    uint8_t show = 0U;

    switch (data[0]) {
    case CMD_IDENTIFY:
        if (length == 1U) {
            i2c_set_response_u16_le(DEVICE_TYPE_NEOPIXEL |
                                    (DEVICE_MODEL_PA0 << 8));
            return;
        }
        break;

    case CMD_RED:
    case CMD_GREEN:
    case CMD_BLUE:
    case CMD_WHITE:
    case CMD_OFF:
        if (length == 1U) {
            uint8_t level = (data[0] == CMD_OFF) ? 0U : DEFAULT_COLOR_LEVEL;

            ws2812_fill((data[0] == CMD_RED || data[0] == CMD_WHITE) ? level : 0U,
                          (data[0] == CMD_GREEN || data[0] == CMD_WHITE) ? level : 0U,
                          (data[0] == CMD_BLUE || data[0] == CMD_WHITE) ? level : 0U);
            response = data[0];
            show = 1U;
        }
        break;

    case CMD_FILL_RGB:
        if (length == 4U) {
            ws2812_fill(data[1], data[2], data[3]);
            response = CMD_FILL_RGB;
            show = 1U;
        }
        break;

    case CMD_SET_PIXEL:
        if (length == 5U && data[1] < WS2812_LED_COUNT) {
            ws2812_set(data[1], data[2], data[3], data[4]);
            response = CMD_SET_PIXEL;
        }
        break;

    case CMD_SHOW:
        if (length == 1U) {
            response = CMD_SHOW;
            show = 1U;
        }
        break;

    default:
        break;
    }

    if (show != 0U && !ws2812_show()) {
        response = RESPONSE_INVALID;
    }
    i2c_set_response_u8(response);
}

int main(void)
{
    uint8_t request[I2C_WRITE_BUFFER_SIZE];
    uint8_t length;

    /* Flash needs one wait state at 24 MHz. */
    FLASH_ACR_REG |= FLASH_ACR_LATENCY_1;
    (void)FLASH_ACR_REG;
    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_24MHZ);

    ws2812_tim1_init();
    i2c_set_response_u8(RESPONSE_INVALID);
    i2c_slave_setup();

    ws2812_fill(0U, 0U, 0U);
    (void)ws2812_show();

    while (1) {
        length = i2c_take_write(request, sizeof(request));
        if (length != 0U) {
            process_i2c_write(request, length);
        }
    }
}
