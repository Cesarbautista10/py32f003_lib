#ifndef __ADC_CONTROL_H
#define __ADC_CONTROL_H

#include "main.h"
#include <stdint.h>

// ADC Configuration Constants
#define ADC_SAMPLES_COUNT       8           // Number of samples for filtering
#define ADC_NOISE_THRESHOLD     50          // Noise threshold (values below this are considered noise)

// Function Declarations
void Init_ADC_PA1(void);                    // Initialize ADC on PA1
void Read_ADC_PA1(void);                    // Read ADC value from PA1
uint16_t Get_ADC_Filtered_Value_PA1(void);  // Get filtered ADC value
uint8_t Scale_ADC_To_I2C_Address(uint16_t adc_value);  // Scale ADC to I2C address

// External variable declarations
extern uint16_t adc_pa1_raw;               // Raw ADC value from PA1
extern uint16_t adc_pa1_value;             // Processed ADC value
extern uint16_t adc_filtered_value;        // Filtered ADC value
extern uint32_t adc_last_read;             // Timestamp of last ADC read
extern uint8_t adc_initialized;            // ADC initialization flag

// ADC filtering variables
extern uint16_t adc_samples[ADC_SAMPLES_COUNT]; // Array for moving average
extern uint32_t adc_sample_sum;            // Sum of samples for moving average
extern uint8_t adc_sample_index;           // Current index in sample array
extern uint8_t adc_samples_filled;         // Flag indicating if sample array is full

#endif // __ADC_CONTROL_H