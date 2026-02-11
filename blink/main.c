#include "main.h"
#include "py32f0xx_hal.h"  
#include "py32f0xx_hal_conf.h"
#include "py32f0xx_it.h"
#include "systemConfig.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════
//                   UART ECHO CONFIGURATION
// ═══════════════════════════════════════════════════════════
#define UART_BAUDRATE       115200  // Velocidad del puerto serial (con oversampling 8)

// Dirección del UID del PY32F003 (96 bits = 12 bytes)
#define UID_BASE_ADDRESS    0x1FFF0E00UL

// Variables globales
UART_HandleTypeDef huart2;          // Usando USART2 en lugar de USART1
uint8_t rx_buffer[1];               // Buffer para recibir 1 byte
uint8_t hello_sent = 0;             // Flag para enviar "Hello" solo una vez

// Declaraciones de funciones
static void Init_UART(void);
static void Init_LED_Debug(void);
static void Send_Hello(void);
static void Send_UID(void);
static void UART_SendString(const char* str);

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

  // Inicializar LED de debug en PB5
  Init_LED_Debug();
  
  // Paso 1: Encender LED para indicar que el programa arrancó
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(500);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(200);

  // Paso 2: Parpadear 2 veces - antes de Init_UART
  for(int i = 0; i < 2; i++) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    HAL_Delay(100);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    HAL_Delay(100);
  }

  // Inicialización del UART
  Init_UART();

  // Paso 3: Parpadear 3 veces rápido - después de Init_UART
  for(int i = 0; i < 3; i++) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    HAL_Delay(100);
  }

  // Paso 4: Enviar mensaje "Hello" al inicio
  Send_Hello();
  
  // Paso 5: Parpadear 5 veces - después de Send_Hello
  for(int i = 0; i < 5; i++) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    HAL_Delay(100);
  }

  // Iniciar recepción en modo interrupción
  HAL_UART_Receive_IT(&huart2, rx_buffer, 1);

  // Loop principal - parpadear lento para indicar que está vivo
  while (1)
  {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    HAL_Delay(1000);
  }
}

/**
  * @brief  Inicialización del LED de debug en PB5
  * @retval None
  */
static void Init_LED_Debug(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  __HAL_RCC_GPIOB_CLK_ENABLE();
  
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}

/**
  * @brief  Inicialización del UART2 en PA0 (TX) y PA1 (RX) - IGUAL QUE RUST
  * @retval None
  */
static void Init_UART(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // Debug 1: Encender LED antes de habilitar relojes
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(100);

  // Habilitar reloj de GPIOA
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // Debug 2: Parpadear después de habilitar GPIOA
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(100);
  
  // Habilitar reloj de USART2
  __HAL_RCC_USART2_CLK_ENABLE();

  // Debug 3: Parpadear después de habilitar USART2 clock
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(100);

  // Configurar PA0 como UART2_TX con AF9 (IGUAL QUE RUST)
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_USART2;  // AF9 para USART2
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Debug 4: Parpadear después de configurar PA0
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(100);

  // Configurar PA1 como UART2_RX con AF9 (IGUAL QUE RUST)
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_USART2;  // AF9 para USART2
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Debug 5: Parpadear después de configurar PA1
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(100);

  // Configurar parámetros del UART2
  huart2.Instance = USART2;
  huart2.Init.BaudRate = UART_BAUDRATE;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_8;  // Cambio a 8 para mejor precisión en baudrate

  // Debug 6: Parpadear antes de HAL_UART_Init
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(100);

  HAL_StatusTypeDef uart_status = HAL_UART_Init(&huart2);
  
  // Debug 7: Parpadear después de HAL_UART_Init
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(100);
  
  if (uart_status != HAL_OK)
  {
    // Si falla, parpadear rápidamente infinito
    while(1) {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
      HAL_Delay(50);
    }
  }

  // Habilitar interrupción del UART2
  HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  
  // Debug 8: Parpadear al final de Init_UART
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}

/**
  * @brief  Enviar mensaje "Hello\r\n" por UART
  * @retval None
  */
static void Send_Hello(void)
{
  const char* hello_msg = "Hello from C!\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)hello_msg, strlen(hello_msg), 1000);
}

/**
  * @brief  Callback de recepción UART completa
  * @param  huart: Handle del UART
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    // Toggle LED para indicar que recibimos un byte
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    
    // Comando 'u' o 'U' para leer UID
    if (rx_buffer[0] == 'u' || rx_buffer[0] == 'U')
    {
      Send_UID();
    }
    else
    {
      // Echo: enviar de vuelta el byte recibido
      HAL_UART_Transmit(&huart2, rx_buffer, 1, 100);
    }
    
    // Volver a habilitar recepción para el siguiente byte
    HAL_UART_Receive_IT(&huart2, rx_buffer, 1);
  }
}

/**
  * @brief  Enviar string por UART
  * @param  str: String a enviar
  * @retval None
  */
static void UART_SendString(const char* str)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), 1000);
}

/**
  * @brief  Leer y enviar el UID del chip (96 bits = 12 bytes)
  * @retval None
  */
static void Send_UID(void)
{
  uint32_t* uid_ptr = (uint32_t*)UID_BASE_ADDRESS;
  char buffer[64];
  
  UART_SendString("\r\nUID: ");
  
  // El UID del PY32F003 tiene 96 bits (3 palabras de 32 bits)
  for (int i = 0; i < 3; i++)
  {
    uint32_t uid_word = uid_ptr[i];
    
    // Convertir a hexadecimal y enviar
    for (int j = 7; j >= 0; j--)
    {
      uint8_t nibble = (uid_word >> (j * 4)) & 0xF;
      char hex_char = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
      HAL_UART_Transmit(&huart2, (uint8_t*)&hex_char, 1, 100);
    }
  }
  
  UART_SendString("\r\n");
}
