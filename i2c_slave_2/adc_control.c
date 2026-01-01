#include "adc_control.h"
#include "main.h"
#include <stdint.h>

// Global ADC handle (defined in main.c)
extern ADC_HandleTypeDef hadc;

// ADC variables (defined here)
uint16_t adc_pa1_raw = 0;                      // Raw ADC value from PA1
uint16_t adc_pa1_value = 0;                    // Processed ADC value
uint16_t adc_filtered_value = 0;               // Filtered ADC value
uint32_t adc_last_read = 0;                    // Timestamp of last ADC read
uint8_t adc_initialized = 0;                   // ADC initialization flag

// ADC filtering variables
uint16_t adc_samples[ADC_SAMPLES_COUNT] = {0}; // Array for moving average
uint32_t adc_sample_sum = 0;                   // Sum of samples for moving average
uint8_t adc_sample_index = 0;                  // Current index in sample array
uint8_t adc_samples_filled = 0;                // Flag indicating if sample array is full

/**
 * @brief Initialize ADC on PA1 for 12-bit readings
 */
void Init_ADC_PA1(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  ADC_ChannelConfTypeDef sConfig = {0};
  
  // Enable clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC_CLK_ENABLE();
  
  // Configure PA1 as analog input
  GPIO_InitStruct.Pin = GPIO_PIN_1;              // PA1 - ADC Channel 1
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;       // Analog mode
  GPIO_InitStruct.Pull = GPIO_NOPULL;            // No pull resistors for ADC
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // Configure ADC peripheral - simplified for PY32F003
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;     // Fast clock for quick response
  hadc.Init.Resolution = ADC_RESOLUTION_12B;               // 12-bit resolution (0-4095)
  hadc.Init.ScanConvMode = ADC_SCAN_ENABLE;                // Scan mode for PY32F003
  hadc.Init.ContinuousConvMode = DISABLE;                  // Single conversion
  hadc.Init.DiscontinuousConvMode = DISABLE;               // Continuous mode
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;         // Software trigger
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;               // Right alignment
  
  if (HAL_ADC_Init(&hadc) != HAL_OK) {
    while(1) {} // ADC initialization error
  }
  
  // Calibrate ADC for better accuracy
  if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK) {
    while(1) {} // ADC calibration error
  }
  
  // Configure ADC channel
  sConfig.Channel = ADC_CHANNEL_1;                         // PA1 = ADC Channel 1
  sConfig.Rank = 1;                                        // First conversion
  sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;         // Balanced timing: precision vs speed
  
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) {
    while(1) {} // Channel configuration error
  }
  
  // Set initialization flag
  adc_initialized = 1;
  
  // Take initial reading for stabilization
  Read_ADC_PA1();
}

/**
 * @brief Read ADC value from PA1 (simplified, no filtering)
 */
void Read_ADC_PA1(void)
{
  // Check if ADC is initialized
  if (adc_initialized == 0) {
    return;
  }
  
  uint16_t raw_value = 0;
  
  // Single ADC reading
  if (HAL_ADC_Start(&hadc) == HAL_OK) {
    // Wait for conversion to complete (max 20ms timeout)
    if (HAL_ADC_PollForConversion(&hadc, 20) == HAL_OK) {
      raw_value = (uint16_t)HAL_ADC_GetValue(&hadc);
    }
    HAL_ADC_Stop(&hadc);
  }
  
  // Store raw and processed values (same value, no filtering)
  adc_pa1_raw = raw_value;
  adc_pa1_value = raw_value;
  adc_filtered_value = raw_value;  // Keep for compatibility
  
  // Update timestamp
  adc_last_read = HAL_GetTick();
}

/**
 * @brief Get ADC value (with automatic refresh if needed)
 */
uint16_t Get_ADC_Filtered_Value_PA1(void)
{
  // Refresh if reading is old (>50ms) or not initialized
  if ((HAL_GetTick() - adc_last_read) > 50 || adc_initialized == 0) {
    Read_ADC_PA1();
  }
  return adc_pa1_value;  // Return direct ADC value (no filtering)
}

/**
 * @brief Scale ADC value to I2C address range (7-bit)
 */
uint8_t Scale_ADC_To_I2C_Address(uint16_t adc_value)
{
  // Scale 12-bit ADC (0-4095) to 7-bit I2C address range (8-119)
  // Avoid reserved I2C addresses (0x00-0x07, 0x78-0x7F)
  uint8_t scaled = (uint8_t)((adc_value * 112) / 4095) + 8;
  
  // Ensure it's within valid range
  if (scaled < 8) scaled = 8;
  if (scaled > 119) scaled = 119;
  
  return scaled;
}