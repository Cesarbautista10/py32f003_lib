#ifndef __ADC_CONTROL_H
#define __ADC_CONTROL_H

#include "main.h"
#include <stdint.h>

// ADC Configuration Constants
#define ADC_SAMPLES_COUNT       4           // Reduced from 8 to 4 for faster response
#define ADC_NOISE_THRESHOLD     50          // Noise threshold
#define ADC_CHANGE_THRESHOLD    500         // Threshold for detecting big changes (500 = ~0.4V)
#define ADC_LOW_THRESHOLD       800         // Below this = 0V state (800 = ~0.65V)
#define ADC_HIGH_THRESHOLD      3000        // Above this = 3.3V state (3000 = ~2.4V)

// Function Declarations
void Init_ADC_PA0(void);                    // Initialize ADC on PA0
void Read_ADC_PA0(void);                    // Read ADC value from PA0
uint16_t Get_ADC_Filtered_Value(void);      // Get filtered ADC value
uint8_t Scale_ADC_To_I2C_Address(uint16_t adc_value);  // Scale ADC to I2C address

// External variable declarations
extern uint16_t adc_pa0_raw;               // Raw ADC value from PA0
extern uint16_t adc_pa0_value;             // Processed ADC value
extern uint16_t adc_filtered_value;        // Filtered ADC value
extern uint32_t adc_last_read;             // Timestamp of last ADC read
extern uint8_t adc_initialized;            // ADC initialization flag
extern uint8_t adc_samples_filled;         // Flag indicating if filtering is active
extern uint8_t adc_stabilization_count;    // Counter for adaptive filtering

#endif // __ADC_CONTROL_H