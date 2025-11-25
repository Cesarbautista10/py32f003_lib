/**
 * ═══════════════════════════════════════════════════════════════════════════════════════
 * @file    pwm_control.h
 * @brief   Control PWM para PY32F003 - TIM1_CH3 en PB6
 * @version v5.2
 * @date    Diciembre 2024
 * @author  Refactorización modular con IA
 * @mcu     PY32F003F18
 * ═══════════════════════════════════════════════════════════════════════════════════════
 * FUNCIONALIDAD:
 * - Control de PWM usando TIM1_CH3 en pin PB6
 * - Generación de tonos audibles para buzzer
 * - Configuración de frecuencia variable (100Hz - 2000Hz)
 * - Control de duty cycle y habilitación/deshabilitación
 * ═══════════════════════════════════════════════════════════════════════════════════════
 */

#ifndef __PWM_CONTROL_H
#define __PWM_CONTROL_H

#include "main.h"
#include <stdint.h>

// PWM Configuration Constants
#define PWM_GPIO_PORT           GPIOB               // Puerto GPIO para PWM
#define PWM_GPIO_PIN            GPIO_PIN_6          // Pin PB6 para TIM1_CH3
#define PWM_TIMER               TIM1                // Timer usado para PWM
#define PWM_CHANNEL             3                   // Canal 3 de TIM1
#define PWM_GPIO_AF             0x01                // Función alternativa AF1 para TIM1_CH3

// PWM Frequency Limits
#define PWM_FREQ_MIN            100                 // Frecuencia mínima 100Hz
#define PWM_FREQ_MAX            2000                // Frecuencia máxima 2000Hz
#define PWM_FREQ_DEFAULT        440                 // Frecuencia por defecto (nota LA)

// PWM System Clock
#define PWM_SYSTEM_CLOCK        24000000            // 24MHz sistema clock

// Function Declarations
void PWM_Init_TIM1(void);                          // Initialize PWM on TIM1_CH3
void PWM_Set_Frequency(uint16_t frequency_hz);     // Set PWM frequency
void PWM_Set_Duty_Cycle(uint16_t duty_percent);    // Set PWM duty cycle (0-100%)
void PWM_Enable(void);                             // Enable PWM output
void PWM_Disable(void);                            // Disable PWM output
void PWM_Stop(void);                               // Stop PWM completely
uint16_t PWM_Get_Current_Frequency(void);          // Get current PWM frequency
uint8_t PWM_Is_Enabled(void);                      // Check if PWM is enabled

// External variable declarations
extern uint16_t pwm_current_frequency;             // Current PWM frequency
extern uint16_t pwm_current_duty;                  // Current PWM duty cycle
extern uint8_t pwm_enabled;                       // PWM enabled flag
extern uint8_t pwm_initialized;                   // PWM initialization flag

#endif // __PWM_CONTROL_H
