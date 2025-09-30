// ===================================================================================
// Basic NeoPixel Functions using Hardware-SPI for PY32F0xx                   * v1.0 *
// ===================================================================================
//
// Functions available:
// --------------------
// NEO_init()               init hardware-SPI for NeoPixels on MOSI-pin
// NEO_clearAll()           clear all pixels and update
// NEO_clearPixel(p)        clear pixel p
// NEO_writeColor(p,r,g,b)  write RGB color to pixel p
// NEO_writeHue(p,h,b)      write hue (h=0..191) and brightness (b=0..2) to pixel p
// NEO_update()             update pixels string (write buffer to pixels)
//
// NEO_sendByte(d)          send one data byte via hardware-SPI to pixels string
// NEO_latch()              latch the data sent
//
// SPI MOSI pin mapping (set below in NeoPixel parameters):
// --------------------------------------------------------
// NEO_MAP    0     1     2     3     4     5     6
// MOSI-pin  PA1   PA2   PA3   PA7   PA8   PA12  PB5
//
// Notes:
// ------
// - Connect MOSI-pin (define below) to DIN of the pixels string.
// - Works with most 800kHz addressable LEDs (NeoPixels).
// - Set number of pixels and pixel type in the parameters below!
// - System clock frequency must be 48MHz, 24MHz, or 12MHz.
//
// 2023 by Stefan Wagner:   https://github.com/wagiminator

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stdbool.h>

#ifndef PY32F003x4
#define PY32F003x4
#endif

#include "py32f0xx.h"
// ===================================================================================
// NeoPixel Definitions
// ===================================================================================
#define NEO_MAP         5    // MOSI pin mapping (see above)
#define NEO_GRB               // type of pixels: NEO_GRB or NEO_RGB
#define NEO_COUNT       8   // total number of pixels in the string (optimized for >10)
#define NEO_LATCH_TIME  50    // reduced latch time for better performance

// ===================================================================================
// NeoPixel Functions and Macros
// ===================================================================================
#define DLY_us(us)      for (volatile uint32_t i = 0; i < us * 12; i++);  // Aprox. 1 us at 48 MHz
#define NEO_latch()     DLY_us(NEO_LATCH_TIME)

// Basic functions
void NEO_init(void);
void NEO_sendByte(uint8_t data);
void NEO_update(void);
void NEO_clearAll(void);
void NEO_writeColor(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b);
void NEO_writeHue(uint8_t pixel, uint8_t hue, uint8_t bright);
void NEO_clearPixel(uint8_t pixel);

// Optimized functions for multiple pixels
void NEO_updateRange(uint8_t start_pixel, uint8_t count);
void NEO_fillRange(uint8_t start_pixel, uint8_t count, uint8_t r, uint8_t g, uint8_t b);
void NEO_fillAll(uint8_t r, uint8_t g, uint8_t b);
void NEO_rainbow(uint8_t start_hue, uint8_t hue_step, uint8_t brightness);
void NEO_shift(uint8_t direction);  // 0=left, 1=right
void NEO_fadeAll(uint8_t fade_amount);

// Fast memory operations
void NEO_copyPixel(uint8_t src_pixel, uint8_t dest_pixel);
void NEO_swapPixels(uint8_t pixel1, uint8_t pixel2);

#ifdef __cplusplus
};
#endif
