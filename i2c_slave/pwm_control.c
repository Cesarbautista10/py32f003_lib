/**
 * ═══════════════════════════════════════════════════════════════════════════════════════
 * @file    pwm_control.c
 * @brief   Control PWM para PY32F003 - TIM3_CH1 en PA2
 * @version v5.3
 * @date    Diciembre 2024
 * @author  Refactorización modular con IA
 * @mcu     PY32F003F18
 * ═══════════════════════════════════════════════════════════════════════════════════════
 */

#include "pwm_control.h"

// ═══════════════════════════════════════════════════════════════════════════════════════
//                               VARIABLES GLOBALES PWM
// ═══════════════════════════════════════════════════════════════════════════════════════
uint16_t pwm_current_frequency = PWM_FREQ_DEFAULT; // Frecuencia actual del PWM
uint16_t pwm_current_duty = 50;                    // Duty cycle actual (0-100%)
uint8_t pwm_enabled = 0;                          // Flag de PWM habilitado
uint8_t pwm_initialized = 0;                      // Flag de PWM inicializado

// Variables internas para cálculos
static uint16_t pwm_arr_value = 999;              // Auto-reload register value
static uint16_t pwm_prescaler = 23;               // Prescaler value

// ═══════════════════════════════════════════════════════════════════════════════════════
//                               FUNCIONES PWM
// ═══════════════════════════════════════════════════════════════════════════════════════

/**
 * @brief  Inicializar PWM en TIM3_CH1 (PA2)
 * @param  None
 * @retval None
 */
void PWM_Init_TIM1(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // Habilitar relojes
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  
  // Configurar PA2 como función alternativa para TIM3_CH1 (PWM)
  // PA2 conecta a TIM3_CH1 usando AF13 en PY32F003
  GPIO_InitStruct.Pin = PWM_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;        // Push-Pull Alternate Function
  GPIO_InitStruct.Pull = GPIO_NOPULL;            // Sin pull resistors
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;  // Alta frecuencia para PWM
  GPIO_InitStruct.Alternate = PWM_GPIO_AF;       // AF13 para TIM3_CH1
  HAL_GPIO_Init(PWM_GPIO_PORT, &GPIO_InitStruct);
  
  // Resetear TIM3 para configuración limpia
  PWM_TIMER->CR1 = 0;
  PWM_TIMER->CR2 = 0;
  PWM_TIMER->CCMR1 = 0;
  PWM_TIMER->CCMR2 = 0;
  PWM_TIMER->CCER = 0;
  PWM_TIMER->CCR1 = 0;
  PWM_TIMER->CCR2 = 0;
  PWM_TIMER->CCR3 = 0;
  PWM_TIMER->CCR4 = 0;
  
  // Configuración básica del timer TIM3
  // Frecuencia del PWM: 24MHz / ((PSC+1) * (ARR+1))
  // Para 1kHz: 24MHz / (24 * 1000) = 1kHz
  pwm_prescaler = 23;        // Prescaler: 24 (divide clock por 24)
  pwm_arr_value = 999;       // Auto-reload: 1000 ciclos
  
  PWM_TIMER->PSC = pwm_prescaler;
  PWM_TIMER->ARR = pwm_arr_value;
  
  // Configurar canal 1 en CCMR1 (canales 1 y 2)
  // Canal 1 usa bits 0-7 en CCMR1
  PWM_TIMER->CCMR1 = 0;                              // Limpiar completamente CCMR1
  PWM_TIMER->CCMR1 |= (0 << 0);                      // CC1S = 00 (salida)
  PWM_TIMER->CCMR1 |= (1 << 3);                      // OC1PE = 1 (preload enable)
  PWM_TIMER->CCMR1 |= (6 << 4);                      // OC1M = 0110 (PWM mode 1)
  PWM_TIMER->CCMR1 |= (0 << 7);                      // OC1CE = 0 (no clear enable)
  
  // Configurar CCER para canal 1
  PWM_TIMER->CCER = 0;                               // Limpiar completamente CCER
  PWM_TIMER->CCER |= (1 << 0);                       // CC1E = 1 (enable channel 1 output)
  PWM_TIMER->CCER &= ~(1 << 1);                      // CC1P = 0 (polaridad activa alta)
  
  // Configurar duty cycle inicial a 0 (apagado)
  PWM_TIMER->CCR1 = 0;                               // 0% - PWM apagado
  
  // Configurar características del timer
  PWM_TIMER->CR1 |= (1 << 7);                        // TIM_CR1_ARPE (Auto-reload preload enable)
  PWM_TIMER->CR1 &= ~(3 << 5);                       // CMS = 00 (Edge-aligned mode)
  PWM_TIMER->CR1 &= ~(1 << 4);                       // DIR = 0 (Upcounter)
  
  // Generar evento de actualización para cargar valores
  PWM_TIMER->EGR |= (1 << 0);                        // UG = 1 (Update generation)
  
  // NO iniciar el timer - permanece apagado hasta recibir comando
  // PWM_TIMER->CR1 |= (1 << 0);                     // CEN = 1 (Counter enable) - DESHABILITADO
  
  // Marcar como inicializado pero NO habilitado
  pwm_initialized = 1;
  pwm_enabled = 0;                                   // PWM APAGADO por defecto
  pwm_current_frequency = PWM_FREQ_DEFAULT;
  pwm_current_duty = 50;
}

/**
 * @brief  Configurar frecuencia del PWM
 * @param  frequency_hz: Frecuencia en Hz (PWM_FREQ_MIN - PWM_FREQ_MAX)
 * @retval None
 */
void PWM_Set_Frequency(uint16_t frequency_hz)
{
  // Validar rango de frecuencia
  if (frequency_hz < PWM_FREQ_MIN) frequency_hz = PWM_FREQ_MIN;
  if (frequency_hz > PWM_FREQ_MAX) frequency_hz = PWM_FREQ_MAX;
  
  // Detener el timer temporalmente
  PWM_TIMER->CR1 &= ~(1 << 0);  // CEN = 0 (Counter disable)
  
  // Calcular prescaler y ARR para la frecuencia deseada
  // Fórmula: freq = 24MHz / ((PSC + 1) * (ARR + 1))
  uint32_t target_period = PWM_SYSTEM_CLOCK / frequency_hz;
  
  uint16_t prescaler = 1;
  uint16_t arr_value = target_period - 1;
  
  // Si el período es muy grande, usar prescaler
  if (target_period > 65535) {
    prescaler = target_period / 65535 + 1;
    arr_value = (target_period / prescaler) - 1;
  }
  
  // Guardar valores calculados
  pwm_prescaler = prescaler - 1;
  pwm_arr_value = arr_value;
  
  // Configurar nuevos valores
  PWM_TIMER->PSC = pwm_prescaler;
  PWM_TIMER->ARR = pwm_arr_value;
  
  // Ajustar duty cycle manteniendo el porcentaje actual
  PWM_TIMER->CCR1 = (pwm_arr_value * pwm_current_duty) / 100;
  
  // Habilitar la salida del canal 1
  PWM_TIMER->CCER |= (1 << 0);                       // CC1E = 1 (habilitar salida canal 1)
  
  // Generar evento de actualización para cargar los nuevos valores
  PWM_TIMER->EGR |= (1 << 0);                        // UG = 1 (Update generation)
  
  // Reiniciar el timer
  PWM_TIMER->CR1 |= (1 << 0);                        // CEN = 1 (Counter enable)
  
  // Actualizar variable de frecuencia actual
  pwm_current_frequency = frequency_hz;
  pwm_enabled = 1;
}

/**
 * @brief  Configurar duty cycle del PWM
 * @param  duty_percent: Duty cycle en porcentaje (0-100)
 * @retval None
 */
void PWM_Set_Duty_Cycle(uint16_t duty_percent)
{
  // Validar rango
  if (duty_percent > 100) duty_percent = 100;
  
  // Calcular y configurar CCR1
  uint16_t ccr_value = (pwm_arr_value * duty_percent) / 100;
  PWM_TIMER->CCR1 = ccr_value;
  
  // Actualizar variable actual
  pwm_current_duty = duty_percent;
  
  // Si duty cycle > 0, asegurar que PWM está habilitado
  if (duty_percent > 0) {
    PWM_Enable();
  }
}

/**
 * @brief  Habilitar salida PWM
 * @param  None
 * @retval None
 */
void PWM_Enable(void)
{
  if (!pwm_initialized) {
    PWM_Init_TIM1();
  }
  
  // Habilitar salida del canal 1
  PWM_TIMER->CCER |= (1 << 0);                       // CC1E = 1 (enable channel 1 output)
  
  // Iniciar el timer si no está corriendo
  PWM_TIMER->CR1 |= (1 << 0);                        // CEN = 1 (Counter enable)
  
  pwm_enabled = 1;
}

/**
 * @brief  Deshabilitar salida PWM (mantener timer corriendo)
 * @param  None
 * @retval None
 */
void PWM_Disable(void)
{
  if (!pwm_enabled) {
    return;  // Ya está deshabilitado, no hacer nada
  }
  
  // Deshabilitar interrupciones temporalmente para operación atómica
  __disable_irq();
  
  // Poner duty cycle a 0 primero
  PWM_TIMER->CCR1 = 0;
  
  // Deshabilitar salida del canal 1
  PWM_TIMER->CCER &= ~(1 << 0);                      // CC1E = 0 (disable channel 1 output)
  
  // Detener completamente el timer para evitar interferencias
  PWM_TIMER->CR1 &= ~(1 << 0);                       // CEN = 0 (Counter disable)
  
  pwm_enabled = 0;
  
  __enable_irq();
  
  // Delay más largo para estabilización del hardware
  for(volatile int i = 0; i < 1000; i++);
}

/**
 * @brief  Parar completamente el PWM
 * @param  None
 * @retval None
 */
void PWM_Stop(void)
{
  // Duty cycle a 0%
  PWM_TIMER->CCR1 = 0;
  
  // Deshabilitar salida del canal 1
  PWM_TIMER->CCER &= ~(1 << 0);                      // CC1E = 0 (disable channel 1 output)
  
  // Detener timer
  PWM_TIMER->CR1 &= ~(1 << 0);                       // CEN = 0 (Counter disable)
  
  pwm_enabled = 0;
}

/**
 * @brief  Obtener frecuencia actual del PWM
 * @param  None
 * @retval uint16_t: Frecuencia actual en Hz
 */
uint16_t PWM_Get_Current_Frequency(void)
{
  return pwm_current_frequency;
}

/**
 * @brief  Verificar si PWM está habilitado
 * @param  None
 * @retval uint8_t: 1 si está habilitado, 0 si está deshabilitado
 */
uint8_t PWM_Is_Enabled(void)
{
  return pwm_enabled;
}
