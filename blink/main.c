#include "main.h"
#include "py32f0xx_hal.h"
#include "systemConfig.h"

// ═══════════════════════════════════════════════════════════
//                   SIMPLE BLINK CONFIGURATION
// ═══════════════════════════════════════════════════════════
#define BLINK_INTERVAL_MS   500    // Intervalo de parpadeo en ms (500ms = 1Hz)

// Variables globales
uint32_t blink_last_toggle = 0;    // Timestamp del último toggle
uint8_t led_state = 0;             // Estado actual del LED (0 = apagado, 1 = encendido)

// Declaraciones de funciones
static void Init_PB5_GPIO(void);
static void Handle_LED_Blink(void);

/**
  * @brief  Función principal
  * @retval int
  */
int main(void)
{
  // Inicialización del HAL
  HAL_Init();

  // Configuración del reloj del sistema
  APP_SystemClockConfig();

  // Inicialización del GPIO PB5
  Init_PB5_GPIO();

  // Loop principal
  while (1)
  {
    Handle_LED_Blink();
    HAL_Delay(1); // Pequeño delay para no sobrecargar la CPU
  }
}



/**
  * @brief  Inicialización del GPIO PB5 como salida
  * @retval None
  */
static void Init_PB5_GPIO(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // Habilitar el reloj del GPIOB
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // Configurar PB5 como salida push-pull
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Inicializar LED apagado
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}

/**
  * @brief  Maneja el parpadeo del LED
  * @retval None
  */
static void Handle_LED_Blink(void)
{
  uint32_t current_time = HAL_GetTick();

  // Verificar si ha pasado el tiempo suficiente para hacer toggle
  if (current_time - blink_last_toggle >= BLINK_INTERVAL_MS)
  {
    // Cambiar estado del LED
    led_state = !led_state;
    
    // Escribir el nuevo estado al GPIO
    if (led_state)
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);    // Encender LED
    }
    else
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);  // Apagar LED
    }
    
    // Actualizar timestamp del último toggle
    blink_last_toggle = current_time;
  }
}

