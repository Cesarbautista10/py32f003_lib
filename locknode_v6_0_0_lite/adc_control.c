#include "adc_control.h"
#include "main.h"
#include <stdint.h>

// Global ADC handle (defined in main.c)
extern ADC_HandleTypeDef hadc;

// ADC variables (defined here)
uint16_t adc_pa0_raw = 0;                      // Raw ADC value from PA0
uint16_t adc_pa0_value = 0;                    // Processed ADC value
uint16_t adc_filtered_value = 0;               // Filtered ADC value
uint32_t adc_last_read = 0;                    // Timestamp of last ADC read
uint8_t adc_initialized = 0;                   // ADC initialization flag
uint8_t adc_samples_filled = 0;                // Flag indicating if filtering is active
uint8_t adc_stabilization_count = 0;           // Counter for initial stabilization

/**
 * @brief Initialize ADC on PA0 for 12-bit readings
 */
void Init_ADC_PA0(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  ADC_ChannelConfTypeDef sConfig = {0};
  
  // Enable clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC_CLK_ENABLE();
  
  // Configure PA0 as analog input
  GPIO_InitStruct.Pin = GPIO_PIN_0;              // PA0 - ADC Channel 0
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
  sConfig.Channel = ADC_CHANNEL_0;                         // PA0 = ADC Channel 0
  sConfig.Rank = 1;                                        // First conversion
  sConfig.SamplingTime =   ADC_SAMPLETIME_3CYCLES_5;           // Fast sampling: 3 cycles for quick response
  
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) {
    while(1) {} // Channel configuration error
  }
  
  // Initialize filtering variables
  adc_filtered_value = 0;
  adc_samples_filled = 1;  // Mark as immediately ready
  adc_stabilization_count = 0;
  
  // Set initialization flag
  adc_initialized = 1;
  
  // NO initial stabilization - immediate response
  // Single read for first value only
  Read_ADC_PA0();
}

/**
 * @brief Read ADC value with immediate jump detection - NO FILTERING
 */
void Read_ADC_PA0(void)
{
  // Check if ADC is initialized
  if (adc_initialized == 0) {
    return;
  }
  
  uint16_t raw_value = 0;
  uint32_t accumulator = 0;
  uint8_t successful_reads = 0;
  
  // Take 5 quick readings for noise reduction only
  for (int i = 0; i < 5; i++) {
    if (HAL_ADC_Start(&hadc) == HAL_OK) {
      if (HAL_ADC_PollForConversion(&hadc, 2) == HAL_OK) {
        accumulator += (uint16_t)HAL_ADC_GetValue(&hadc);
        successful_reads++;
      }
      HAL_ADC_Stop(&hadc);
    }
  }
  
  // Use average of successful readings
  if (successful_reads > 0) {
    raw_value = (uint16_t)(accumulator / successful_reads);
  } else {
    raw_value = adc_pa0_raw;
  }
  
  // Store raw value
  adc_pa0_raw = raw_value;
  
  // NO FILTERING - Direct state classification
  if (raw_value < ADC_LOW_THRESHOLD) {
    // Low state: force to 0 immediately
    adc_filtered_value = 0;
  } else if (raw_value > ADC_HIGH_THRESHOLD) {
    // High state: force to max immediately  
    adc_filtered_value = 4095;
  } else {
    // Intermediate state: use raw value directly
    adc_filtered_value = raw_value;
  }
  
  // Store final processed value - NO SMOOTHING
  adc_pa0_value = adc_filtered_value;
  
  // Update timestamp
  adc_last_read = HAL_GetTick();
}

/**
 * @brief Get filtered ADC value (with forced fresh reading)
 */
uint16_t Get_ADC_Filtered_Value(void)
{
  // Always force a fresh reading for immediate response
  Read_ADC_PA0();
  return adc_pa0_value;
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