#include "ws2812_tim1.h"

#include <libopencm3/cm3/systick.h>
#include <libopencm3/py32/gpio.h>
#include <libopencm3/py32/memorymap.h>
#include <libopencm3/py32/rcc.h>

#define TIM1_CR1_REG          MMIO32(TIM1_BASE + 0x00U)
#define TIM1_DIER_REG         MMIO32(TIM1_BASE + 0x0CU)
#define TIM1_EGR_REG          MMIO32(TIM1_BASE + 0x14U)
#define TIM1_CCMR2_REG        MMIO32(TIM1_BASE + 0x1CU)
#define TIM1_CCER_REG         MMIO32(TIM1_BASE + 0x20U)
#define TIM1_CNT_REG          MMIO32(TIM1_BASE + 0x24U)
#define TIM1_PSC_REG          MMIO32(TIM1_BASE + 0x28U)
#define TIM1_ARR_REG          MMIO32(TIM1_BASE + 0x2CU)
#define TIM1_CCR3_REG         MMIO32(TIM1_BASE + 0x3CU)
#define TIM1_BDTR_REG         MMIO32(TIM1_BASE + 0x44U)

#define DMA1_ISR_REG          MMIO32(DMA1_BASE + 0x00U)
#define DMA1_IFCR_REG         MMIO32(DMA1_BASE + 0x04U)
#define DMA1_CCR1_REG         MMIO32(DMA1_BASE + 0x08U)
#define DMA1_CNDTR1_REG       MMIO32(DMA1_BASE + 0x0CU)
#define DMA1_CPAR1_REG        MMIO32(DMA1_BASE + 0x10U)
#define DMA1_CMAR1_REG        MMIO32(DMA1_BASE + 0x14U)
#define SYSCFG_CFGR3_REG      MMIO32(SYSCFG_BASE + 0x1CU)

#define TIM_CR1_CEN           (1U << 0)
#define TIM_CR1_ARPE          (1U << 7)
#define TIM_DIER_UDE          (1U << 8)
#define TIM_EGR_UG            (1U << 0)
#define TIM_CCMR2_OC3PE       (1U << 3)
#define TIM_CCMR2_OC3M_PWM1   (6U << 4)
#define TIM_CCER_CC3E         (1U << 8)
#define TIM_BDTR_MOE          (1U << 15)

#define DMA_HTIF1             (1U << 2)
#define DMA_TCIF1             (1U << 1)
#define DMA_TEIF1             (1U << 3)
#define DMA_CLEAR_CH1         0x0FU
#define DMA_CCR_EN            (1U << 0)
#define DMA_CCR_DIR           (1U << 4)
#define DMA_CCR_CIRC          (1U << 5)
#define DMA_CCR_MINC          (1U << 7)
#define DMA_CCR_PSIZE_16      (1U << 8)
#define DMA_CCR_MSIZE_16      (1U << 10)
#define DMA_CCR_PL_HIGH       (2U << 12)
#define DMA_MAP_MASK          0x1FU
#define DMA_MAP_TIM1_UP       0x10U

/* 24 MHz / 30 = 800 kHz; PWM high times are 333 ns and 750 ns. */
#define BIT_PERIOD_TICKS      30U
#define ZERO_HIGH_TICKS       8U
#define ONE_HIGH_TICKS        18U
#define DMA_SLOTS             512U
#define HALF_SLOTS            (DMA_SLOTS / 2U)
#define START_LOW_SLOTS       HALF_SLOTS /* Keep DIN low while DMA starts. */
#define RESET_LOW_SLOTS       240U /* 300 us, above the WS2812 reset minimum. */
#define FRAME_SLOTS           (START_LOW_SLOTS + WS2812_LED_COUNT * 24U + RESET_LOW_SLOTS + 2U)
#define SHOW_TIMEOUT_MS       20U

static uint8_t pixels[WS2812_LED_COUNT][3]; /* Stored in GRB wire order. */
static uint16_t duty_slots[DMA_SLOTS];
static volatile uint32_t system_millis;
static uint16_t start_slots;
static uint8_t pixel_index;
static uint8_t component_index;
static uint8_t bit_mask;

void sys_tick_handler(void)
{
    system_millis++;
}

uint32_t ws2812_millis(void)
{
    return system_millis;
}

static uint16_t next_duty(void)
{
    uint16_t duty;

    if (start_slots != 0U) {
        start_slots--;
        return 0U;
    }
    if (pixel_index >= WS2812_LED_COUNT) {
        return 0U;
    }

    duty = (pixels[pixel_index][component_index] & bit_mask) != 0U ?
           ONE_HIGH_TICKS : ZERO_HIGH_TICKS;
    bit_mask >>= 1;
    if (bit_mask == 0U) {
        bit_mask = 0x80U;
        component_index++;
        if (component_index == 3U) {
            component_index = 0U;
            pixel_index++;
        }
    }
    return duty;
}

static void fill_half(uint16_t offset)
{
    uint16_t index;

    for (index = offset; index < offset + HALF_SLOTS; index++) {
        duty_slots[index] = next_duty();
    }
}

static void stop_stream(void)
{
    TIM1_DIER_REG = 0U;
    DMA1_CCR1_REG &= ~DMA_CCR_EN;
    TIM1_CR1_REG &= ~TIM_CR1_CEN;
    TIM1_CCR3_REG = 0U;
    TIM1_EGR_REG = TIM_EGR_UG;
    DMA1_IFCR_REG = DMA_CLEAR_CH1;
}

void ws2812_tim1_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_TIM1);
    rcc_periph_clock_enable(RCC_DMA1);
    rcc_periph_clock_enable(RCC_SYSCFG);

    gpio_clear(GPIOA, GPIO0);
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO0);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, GPIO0);
    gpio_set_af(GPIOA, GPIO_AF13, GPIO0); /* PA0 = TIM1_CH3. */

    TIM1_CR1_REG = 0U;
    TIM1_DIER_REG = 0U;
    TIM1_PSC_REG = 0U;
    TIM1_ARR_REG = BIT_PERIOD_TICKS - 1U;
    TIM1_CCR3_REG = 0U;
    TIM1_CCMR2_REG = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
    TIM1_CCER_REG = TIM_CCER_CC3E;
    TIM1_BDTR_REG = TIM_BDTR_MOE;
    TIM1_EGR_REG = TIM_EGR_UG;

    DMA1_CCR1_REG = 0U;
    SYSCFG_CFGR3_REG = (SYSCFG_CFGR3_REG & ~DMA_MAP_MASK) | DMA_MAP_TIM1_UP;
    DMA1_CPAR1_REG = (uint32_t)(TIM1_BASE + 0x3CU);
    DMA1_CMAR1_REG = (uint32_t)duty_slots;
    DMA1_IFCR_REG = DMA_CLEAR_CH1;

    systick_set_frequency(1000U, rcc_ahb_frequency);
    systick_clear();
    systick_interrupt_enable();
    systick_counter_enable();
}

void ws2812_fill(uint8_t red, uint8_t green, uint8_t blue)
{
    uint16_t index;

    for (index = 0U; index < WS2812_LED_COUNT; index++) {
        ws2812_set((uint8_t)index, red, green, blue);
    }
}

void ws2812_set(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (index >= WS2812_LED_COUNT) {
        return;
    }

    pixels[index][0] = green;
    pixels[index][1] = red;
    pixels[index][2] = blue;
}

bool ws2812_show(void)
{
    uint32_t start_ms;
    uint16_t consumed = 0U;
    uint32_t flags;
    bool success = true;

    start_slots = START_LOW_SLOTS;
    pixel_index = 0U;
    component_index = 0U;
    bit_mask = 0x80U;
    fill_half(0U);
    fill_half(HALF_SLOTS);

    TIM1_CR1_REG = 0U;
    TIM1_DIER_REG = 0U;
    TIM1_CNT_REG = 0U;
    TIM1_CCR3_REG = 0U;
    TIM1_EGR_REG = TIM_EGR_UG;
    DMA1_CCR1_REG = 0U;
    DMA1_CNDTR1_REG = DMA_SLOTS;
    DMA1_CMAR1_REG = (uint32_t)duty_slots;
    DMA1_IFCR_REG = DMA_CLEAR_CH1;
    /* Each update DMA write prepares CCR3 for the following PWM period. */
    DMA1_CCR1_REG = DMA_CCR_DIR | DMA_CCR_CIRC | DMA_CCR_MINC |
                    DMA_CCR_PSIZE_16 | DMA_CCR_MSIZE_16 | DMA_CCR_PL_HIGH |
                    DMA_CCR_EN;
    start_ms = system_millis;
    TIM1_DIER_REG = TIM_DIER_UDE;
    TIM1_CR1_REG = TIM_CR1_ARPE | TIM_CR1_CEN;

    while (consumed < FRAME_SLOTS) {
        flags = DMA1_ISR_REG;
        if ((flags & DMA_TEIF1) != 0U ||
            (flags & (DMA_HTIF1 | DMA_TCIF1)) == (DMA_HTIF1 | DMA_TCIF1)) {
            success = false;
            break;
        }
        if ((flags & DMA_HTIF1) != 0U) {
            DMA1_IFCR_REG = DMA_HTIF1;
            consumed += HALF_SLOTS;
            fill_half(0U);
        } else if ((flags & DMA_TCIF1) != 0U) {
            DMA1_IFCR_REG = DMA_TCIF1;
            consumed += HALF_SLOTS;
            fill_half(HALF_SLOTS);
        }
        if ((uint32_t)(system_millis - start_ms) > SHOW_TIMEOUT_MS) {
            success = false;
            break;
        }
    }

    stop_stream();
    return success;
}
