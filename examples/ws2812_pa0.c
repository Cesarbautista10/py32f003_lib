#include <stdint.h>

#include <libopencm3/py32/memorymap.h>
#include <libopencm3/py32/rcc.h>

#include "ws2812_tim1.h"

#define FLASH_ACR_REG        MMIO32(FLASH_MEM_INTERFACE_BASE + 0x00U)
#define FLASH_ACR_LATENCY_1  (1U << 0)
#define COLOR_LEVEL          32U
#define SOLID_DELAY_MS       600U
#define CHASE_DELAY_MS       60U

static void delay_ms(uint32_t duration)
{
    uint32_t start = ws2812_millis();

    while ((uint32_t)(ws2812_millis() - start) < duration) {
        /* SysTick keeps running while the frame is displayed. */
    }
}

static void show_solid(uint8_t red, uint8_t green, uint8_t blue)
{
    ws2812_fill(red, green, blue);
    (void)ws2812_show();
    delay_ms(SOLID_DELAY_MS);
}

int main(void)
{
    uint16_t index;

    FLASH_ACR_REG |= FLASH_ACR_LATENCY_1;
    (void)FLASH_ACR_REG;
    rcc_clock_setup_in_hsi_out(RCC_ICSCR_HSI_FS_24MHZ);
    ws2812_tim1_init();

    while (1) {
        show_solid(COLOR_LEVEL, 0U, 0U);
        show_solid(0U, COLOR_LEVEL, 0U);
        show_solid(0U, 0U, COLOR_LEVEL);
        show_solid(COLOR_LEVEL, COLOR_LEVEL, COLOR_LEVEL);

        /* A moving white pixel checks the data order along the strip. */
        for (index = 0U; index < WS2812_LED_COUNT; index++) {
            ws2812_fill(0U, 0U, 0U);
            ws2812_set((uint8_t)index, COLOR_LEVEL, COLOR_LEVEL, COLOR_LEVEL);
            (void)ws2812_show();
            delay_ms(CHASE_DELAY_MS);
        }
    }
}
