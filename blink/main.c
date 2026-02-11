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
ADC_HandleTypeDef hadc;              // Handle del ADC
uint8_t rx_buffer[1];               // Buffer para recibir 1 byte
uint8_t hello_sent = 0;             // Flag para enviar "Hello" solo una vez

// Declaraciones de funciones
static void Init_UART(void);
static void Init_LED_Debug(void);
static void Init_ADC(void);
static void Send_Hello(void);
static void Send_UID(void);
static void Send_ADC_Value(void);
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
  
  // Inicializar ADC en PA2
  Init_ADC();
  
  // Paso 1: Encender LED para indicar que el programa arrancó
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(500);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(200);

  // // Paso 2: Parpadear 2 veces - antes de Init_UART
  // for(int i = 0; i < 2; i++) {
  //   HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
  //   HAL_Delay(100);
  //   HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
  //   HAL_Delay(100);
  // }

  // Inicialización del UART
  Init_UART();

  // // Paso 3: Parpadear 3 veces rápido - después de Init_UART
  // for(int i = 0; i < 3; i++) {
  //   HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
  //   HAL_Delay(100);
  // }

  // Paso 4: Enviar mensaje "Hello" al inicio
  Send_Hello();
  
  // // Paso 5: Parpadear 5 veces - después de Send_Hello
  // for(int i = 0; i < 5; i++) {
  //   HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
  //   HAL_Delay(100);
  // }

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
  * @brief  Inicialización del ADC en PA2 (canal 2)
  * @retval None
  */
static void Init_ADC(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  // Habilitar reloj de GPIOA
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // Habilitar reloj del ADC
  __HAL_RCC_ADC_CLK_ENABLE();

  // Configurar PA2 como entrada analógica con pull-down
  // El pull-down evita lecturas flotantes cuando no hay señal conectada
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;  // Pull-down para evitar ruido cuando está flotando
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Configurar el ADC
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc.Init.SamplingTimeCommon = ADC_SAMPLETIME_239CYCLES_5;
  
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    // Error: parpadear rápido
    while(1) {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
      HAL_Delay(25);
    }
  }

  // Configurar el canal ADC (PA2 = ADC_IN2)
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    // Error: parpadear muy rápido
    while(1) {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
      HAL_Delay(10);
    }
  }
  
  // Calibrar el ADC
  if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK)
  {
    // Error de calibración: parpadear medio rápido
    while(1) {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
      HAL_Delay(15);
    }
  }
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
  const char* hello_msg2 =  " \r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)hello_msg2, strlen(hello_msg2), 10);
  const char* hello_msg = "Keep Calm, Evelyn!\r\n";
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
    // Guardar el caracter recibido
    uint8_t received_char = rx_buffer[0];
    
    // PRIMERO: Re-habilitar recepción inmediatamente
    HAL_UART_Receive_IT(&huart2, rx_buffer, 1);
    
    // DESPUÉS: Procesar el comando
    // Toggle LED para indicar que recibimos un byte
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    
    // Comando 'u' o 'U' para leer UID
    if (received_char == 'u' || received_char == 'U')
    {
      Send_UID();
    }
    // Comando 'a' o 'A' para leer ADC
    else if (received_char == 'a' || received_char == 'A')
    {
      Send_ADC_Value();
    }
    else
    {
      // Echo: enviar de vuelta el byte recibido
      HAL_UART_Transmit(&huart2, &received_char, 1, 100);
    }
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
  
  UART_SendString("UID: ");
  
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

/**
  * @brief  Leer y enviar el valor del ADC en PA2
  * @retval None
  */
static void Send_ADC_Value(void)
{
  char buffer[32];
  uint32_t adc_value = 0;
  uint32_t voltage_mv = 0;
  HAL_StatusTypeDef status;
  
  // Iniciar conversión ADC
  status = HAL_ADC_Start(&hadc);
  if (status != HAL_OK)
  {
    UART_SendString("ADC Start Error\r\n");
    return;
  }
  
  // Esperar a que termine la conversión (timeout 10ms es suficiente)
  if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK)
  {
    // Leer el valor del ADC (12 bits: 0-4095)
    adc_value = HAL_ADC_GetValue(&hadc);
    
    // Calcular voltaje en mV (asumiendo Vref = 5V)
    // voltage_mv = (adc_value * 5000) / 4095
    voltage_mv = (adc_value * 5000) / 4095;
    
    // Formatear y enviar el resultado
    UART_SendString("ADC PA2: ");
    
    // Enviar valor ADC (0-4095)
    int len = 0;
    uint32_t temp = adc_value;
    do {
      buffer[len++] = '0' + (temp % 10);
      temp /= 10;
    } while (temp > 0);
    
    // Invertir el string
    for (int i = 0; i < len / 2; i++) {
      char c = buffer[i];
      buffer[i] = buffer[len - 1 - i];
      buffer[len - 1 - i] = c;
    }
    buffer[len] = '\0';
    UART_SendString(buffer);
    
    UART_SendString(" | ");
    
    // Enviar voltaje en mV
    len = 0;
    temp = voltage_mv;
    do {
      buffer[len++] = '0' + (temp % 10);
      temp /= 10;
    } while (temp > 0);
    
    // Invertir el string
    for (int i = 0; i < len / 2; i++) {
      char c = buffer[i];
      buffer[i] = buffer[len - 1 - i];
      buffer[len - 1 - i] = c;
    }
    buffer[len] = '\0';
    UART_SendString(buffer);
    UART_SendString(" mV\r\n");
  }
  else
  {
    UART_SendString("ADC Timeout\r\n");
  }
  
  // Detener el ADC y limpiar flags
  HAL_ADC_Stop(&hadc);
  
  // Limpiar cualquier flag pendiente
  __HAL_ADC_CLEAR_FLAG(&hadc, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);
}
