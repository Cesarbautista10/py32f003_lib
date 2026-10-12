#ifndef WS2812_TIM1_H
#define WS2812_TIM1_H

#include <stdbool.h>
#include <stdint.h>

#ifndef WS2812_LED_COUNT
#define WS2812_LED_COUNT 80U
#endif

#if WS2812_LED_COUNT == 0U || WS2812_LED_COUNT > 200U
#error "WS2812_LED_COUNT must be between 1 and 200 to leave stack space in 2 KB RAM"
#endif

void ws2812_tim1_init(void);
uint32_t ws2812_millis(void);
void ws2812_fill(uint8_t red, uint8_t green, uint8_t blue);
void ws2812_set(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
bool ws2812_show(void);

#endif
