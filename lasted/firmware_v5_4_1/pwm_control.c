/**
 * ═══════════════════════════════════════════════════════════════════════════════════════
 * @file    pwm_control.c
 * @brief   Control PWM para PY32F003 - TIM1_CH3 en PB6
 * @version v5.2
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
 * @brief  Inicializar PWM en TIM1_CH3 (PB6)
 * @param  None
 * @retval None
 */
void PWM_Init_TIM1(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // Habilitar relojes
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();
  
  // Configurar PB6 como función alternativa para TIM1_CH3 (PWM)
  // PB6 conecta a TIM1_CH3 usando AF1 en PY32F003 (CORREGIDO)
  GPIO_InitStruct.Pin = PWM_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;        // Push-Pull Alternate Function
  GPIO_InitStruct.Pull = GPIO_NOPULL;            // Sin pull resistors
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;  // Alta frecuencia para PWM
  GPIO_InitStruct.Alternate = PWM_GPIO_AF;       // AF1 para TIM1_CH3 (CORREGIDO)
  HAL_GPIO_Init(PWM_GPIO_PORT, &GPIO_InitStruct);
  
  // Resetear TIM1 para configuración limpia
  PWM_TIMER->CR1 = 0;
  PWM_TIMER->CR2 = 0;
  PWM_TIMER->CCMR1 = 0;
  PWM_TIMER->CCMR2 = 0;
  PWM_TIMER->CCER = 0;
  PWM_TIMER->BDTR = 0;
  PWM_TIMER->CCR1 = 0;
  PWM_TIMER->CCR2 = 0;
  PWM_TIMER->CCR3 = 0;
  PWM_TIMER->CCR4 = 0;
  
  // Configuración básica del timer TIM1
  // Frecuencia del PWM: 24MHz / ((PSC+1) * (ARR+1))
  // Para 1kHz: 24MHz / (24 * 1000) = 1kHz
  pwm_prescaler = 23;        // Prescaler: 24 (divide clock por 24)
  pwm_arr_value = 999;       // Auto-reload: 1000 ciclos
  
  PWM_TIMER->PSC = pwm_prescaler;
  PWM_TIMER->ARR = pwm_arr_value;
  
  // Configurar canal 3 en CCMR2 (canales 3 y 4)
  // Canal 3 usa bits 0-7 en CCMR2
  PWM_TIMER->CCMR2 = 0;                              // Limpiar completamente CCMR2
  PWM_TIMER->CCMR2 |= (0 << 0);                      // CC3S = 00 (salida)
  PWM_TIMER->CCMR2 |= (1 << 3);                      // OC3PE = 1 (preload enable)
  PWM_TIMER->CCMR2 |= (6 << 4);                      // OC3M = 0110 (PWM mode 1)
  PWM_TIMER->CCMR2 |= (0 << 7);                      // OC3CE = 0 (no clear enable)
  
  // Configurar CCER para canal 3
  PWM_TIMER->CCER = 0;                               // Limpiar completamente CCER
  PWM_TIMER->CCER |= (1 << 8);                       // CC3E = 1 (enable channel 3 output)
  PWM_TIMER->CCER &= ~(1 << 9);                      // CC3P = 0 (polaridad activa alta)
  PWM_TIMER->CCER &= ~(1 << 10);                     // CC3NE = 0 (disable complementary output)
  PWM_TIMER->CCER &= ~(1 << 11);                     // CC3NP = 0 (complementary polarity)
  
  // Configurar duty cycle inicial (50% para prueba audible)
  PWM_TIMER->CCR3 = pwm_arr_value / 2;               // 50% de ARR
  
  // Configurar características avanzadas de TIM1
  PWM_TIMER->CR1 |= (1 << 7);                        // TIM_CR1_ARPE (Auto-reload preload enable)
  PWM_TIMER->CR1 &= ~(3 << 5);                       // CMS = 00 (Edge-aligned mode)
  PWM_TIMER->CR1 &= ~(1 << 4);                       // DIR = 0 (Upcounter)
  
  //  CRÍTICO: TIM1 requiere MOE (Main Output Enable) para advanced timers
  PWM_TIMER->BDTR |= (1 << 15);                      // MOE = 1 (Main Output Enable)
  PWM_TIMER->BDTR &= ~(0xFF << 0);                   // DTG = 0 (No dead time)
  PWM_TIMER->BDTR &= ~(3 << 10);                     // LOCK = 00 (No lock)
  
  // Generar evento de actualización para cargar valores
  PWM_TIMER->EGR |= (1 << 0);                        // UG = 1 (Update generation)
  
  // Iniciar el timer
  PWM_TIMER->CR1 |= (1 << 0);                        // CEN = 1 (Counter enable)
  
  // Marcar como inicializado y habilitado
  pwm_initialized = 1;
  pwm_enabled = 1;
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
  PWM_TIMER->CCR3 = (pwm_arr_value * pwm_current_duty) / 100;
  
  // Habilitar la salida del canal 3 (importante para reactivar después de PWM_Stop)
  PWM_TIMER->CCER |= (1 << 8);                       // CC3E = 1 (habilitar salida canal 3)
  
  // Asegurar que MOE está habilitado (requerido para TIM1)
  PWM_TIMER->BDTR |= (1 << 15);                      // MOE = 1 (Main Output Enable)
  
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
  
  // Calcular y configurar CCR3
  uint16_t ccr_value = (pwm_arr_value * duty_percent) / 100;
  PWM_TIMER->CCR3 = ccr_value;
  
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
  
  // Habilitar salida del canal 3
  PWM_TIMER->CCER |= (1 << 8);                       // CC3E = 1 (enable channel 3 output)
  
  // Asegurar que MOE está habilitado (requerido para TIM1)
  PWM_TIMER->BDTR |= (1 << 15);                      // MOE = 1 (Main Output Enable)
  
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
  // Deshabilitar salida del canal 3
  PWM_TIMER->CCER &= ~(1 << 8);                      // CC3E = 0 (disable channel 3 output)
  
  pwm_enabled = 0;
}

/**
 * @brief  Parar completamente el PWM
 * @param  None
 * @retval None
 */
void PWM_Stop(void)
{
  // Duty cycle a 0%
  PWM_TIMER->CCR3 = 0;
  
  // Deshabilitar salida del canal 3
  PWM_TIMER->CCER &= ~(1 << 8);                      // CC3E = 0 (disable channel 3 output)
  
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
