#include "main.h"
#include "py32f0xx_hal.h"  
#include "py32f0xx_hal_conf.h"
#include "py32f0xx_it.h"
#include <stdlib.h>
#include "commands.h"
#include "systemConfig.h"
#include "pwm_control.h"
#include "neo_spi.h"

#define FLASH_SAVE_ADDR     0x08003C00  
#define FLASH_I2C_ADDR      0x08003E00  
#define FLASH_CONFIG_ADDR   0x08003F00  
#define CONFIG_USE_FLASH_I2C   0x01     
#define CONFIG_FACTORY_DEFAULT 0x02     

I2C_HandleTypeDef I2cHandle;
uint8_t aTxBuffer[1] = {0};
uint8_t aRxBuffer[1] = {0};
uint8_t state_button = 0;
ADC_HandleTypeDef hadc;              
uint8_t pa0_digital_state = 0;       
uint32_t pa0_last_read = 0;          
uint8_t pa0_initialized = 0;         
uint32_t watchdog_timeout_ms = 2000;    
uint32_t watchdog_last_refresh = 0;  
uint32_t system_uptime = 0;          
uint8_t reset_requested = 0;         
uint32_t last_command_time = 0;      
uint8_t system_stable = 1;           
uint8_t flash_save_flag = 0;         
uint8_t pulse_pending = 0;           
uint32_t pulse_start_time = 0;       
uint8_t pulse_active = 0;            
uint8_t use_flash_i2c = 0;           
uint8_t current_i2c_address = 0x00;  
uint8_t factory_default_addr = 0x00;
volatile uint8_t i2c_rx_complete = 0;
volatile uint8_t i2c_tx_complete = 0;
int error_count = 0;
static void Init_i2c_pins(void);
static void Init_PA2_PB5_Relay(void);     
static void Pulse_Relays(void);           
static void Process_Relay_Pulse(void);    
static void Init_PA0_Digital(void);       
static void Read_PA0_Digital(void);       
static void Init_PF4_LED(void);            // LED builtin en PF4
static void Process_I2C_Command(void);
static uint8_t Build_Response_With_PA0(uint8_t base_response);  
static void I2C_Recovery(void);
static void Init_Watchdog(void);          
static void Refresh_Watchdog(void);       
static void Force_System_Reset(void);     
static void Disable_Watchdog_Temporarily(void);  
static uint32_t Get_Device_UID(void);
static uint8_t Calculate_I2C_Address(uint32_t uid);
static uint8_t Calculate_I2C_Address_Internal(uint32_t uid);


void Save_DATA_To_Flash(uint16_t data)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t PageError = 0;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = FLASH_SAVE_ADDR;
    EraseInitStruct.NbPages = 1;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) {
        HAL_FLASH_Lock();
        return;
    }
    uint32_t data32 = (uint32_t)data;  
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, FLASH_SAVE_ADDR, &data32);
    HAL_FLASH_Lock();
}
uint16_t Read_DATA_From_Flash(void)
{
    return *(uint16_t*)FLASH_SAVE_ADDR;
}
void Save_I2C_Address_To_Flash(uint8_t address)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t PageError = 0;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = FLASH_I2C_ADDR;
    EraseInitStruct.NbPages = 1;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
        uint32_t addr32 = (uint32_t)address;
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, FLASH_I2C_ADDR, &addr32);
    }
    HAL_FLASH_Lock();
}
uint8_t Read_I2C_Address_From_Flash(void)
{
    uint16_t stored_addr = *(uint16_t*)FLASH_I2C_ADDR;
    if (stored_addr >= I2C_ADDR_MIN && stored_addr <= I2C_ADDR_MAX) {
        return (uint8_t)stored_addr;
    }
    return 0x00; 
}
void Save_Config_To_Flash(uint8_t config_flags)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t PageError = 0;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = FLASH_CONFIG_ADDR;
    EraseInitStruct.NbPages = 1;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
        uint32_t config32 = (uint32_t)config_flags;
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, FLASH_CONFIG_ADDR, &config32);
    }
    HAL_FLASH_Lock();
}
uint8_t Read_Config_From_Flash(void)
{
    uint16_t stored_config = *(uint16_t*)FLASH_CONFIG_ADDR;
    return (uint8_t)(stored_config & 0xFF);
}
uint8_t Init_I2C_Address_Management(void)
{
    uint8_t config = Read_Config_From_Flash();
    uint32_t device_uid = Get_Device_UID();
    factory_default_addr = Calculate_I2C_Address(device_uid);
    if (config & CONFIG_USE_FLASH_I2C) {
        uint8_t flash_addr = Read_I2C_Address_From_Flash();
        if (flash_addr != 0x00) {
            use_flash_i2c = 1;
            current_i2c_address = flash_addr;
            return flash_addr;
        }
    }
    use_flash_i2c = 0;
    current_i2c_address = factory_default_addr;
    return factory_default_addr;
}
void Set_Custom_I2C_Address(uint8_t new_address)
{
    if (new_address >= I2C_ADDR_MIN && new_address <= I2C_ADDR_MAX) {
        Save_I2C_Address_To_Flash(new_address);
        Save_Config_To_Flash(CONFIG_USE_FLASH_I2C);
        use_flash_i2c = 1;
        current_i2c_address = new_address;
    }
}
void Reset_To_Factory_I2C(void)
{
    Save_Config_To_Flash(CONFIG_FACTORY_DEFAULT);
    use_flash_i2c = 0;
    current_i2c_address = factory_default_addr;
}
static void Init_i2c_pins(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // Habilitar relojes para I2C
  __HAL_RCC_GPIOF_CLK_ENABLE();                    //  MIGRADO: PF0/PF1 SÍ tienen I2C con AF12
  __HAL_RCC_I2C_CLK_ENABLE();

  //  MIGRADO: Configurar PF0 como SDA (I2C_SDA) - AF12 CONFIRMADO
  GPIO_InitStruct.Pin = GPIO_PIN_0;                // PF0 - SDA (AF12)
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;          // Open Drain para I2C
  GPIO_InitStruct.Pull = GPIO_PULLUP;              // Pull-up interno
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_I2C;       //  AF12 para PF0/PF1 I2C (CONFIRMADO)
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  //  MIGRADO: Configurar PF1 como SCL (I2C_SCL) - AF12 CONFIRMADO
  GPIO_InitStruct.Pin = GPIO_PIN_1;                // PF1 - SCL (AF12)
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;          // Open Drain para I2C
  GPIO_InitStruct.Pull = GPIO_PULLUP;              // Pull-up interno
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_I2C;       //  AF12 para PF0/PF1 I2C (CONFIRMADO)
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}
//  FUNCIÓN: Inicializar PA4 como entrada digital
static void Init_PA0_Digital(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // Habilitar reloj GPIOA
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // Configurar PA4 como entrada digital con pull-up
  GPIO_InitStruct.Pin = GPIO_PIN_4;              // PA4 - Pin digital (MIGRADO desde PA0)
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;        // Entrada digital
  GPIO_InitStruct.Pull = GPIO_PULLUP;            // Pull-up interno para lectura estable
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // Velocidad baja para entrada
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // Marcar como inicializado
  pa0_initialized = 1;
  
  // Hacer primera lectura
  Read_PA0_Digital();
}

/**
 * @brief  Inicializar LED builtin en PF4
 * @note   PF4 como salida digital push-pull para LED de estado
 */
static void Init_PF4_LED(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // Habilitar reloj GPIOF
  __HAL_RCC_GPIOF_CLK_ENABLE();
  
  // Configurar PF4 como salida digital push-pull
  GPIO_InitStruct.Pin = GPIO_PIN_4;              // PF4 - LED builtin
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;    // Salida push-pull
  GPIO_InitStruct.Pull = GPIO_NOPULL;            // Sin pull resistor
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // Velocidad baja para LED
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  
  // Apagar LED inicialmente
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_4, GPIO_PIN_RESET);
}

//  FUNCIÓN: Leer PA4 como entrada digital
static void Read_PA0_Digital(void)
{
  // Asegurar que PA4 está configurado como digital
  if (pa0_initialized == 0) {
    Init_PA0_Digital();
  }
  
  // Leer estado digital de PA4 (MIGRADO desde PA0)
  GPIO_PinState pin_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);
  pa0_digital_state = (pin_state == GPIO_PIN_SET) ? 1 : 0;
  
  // Actualizar timestamp
  pa0_last_read = HAL_GetTick();
}
static void I2C_Recovery(void)
{
  HAL_I2C_DeInit(&I2cHandle);
  HAL_Delay(10);
  HAL_I2C_Init(&I2cHandle);
}
static void Init_Watchdog(void)
{
  watchdog_timeout_ms = 0;              
  watchdog_last_refresh = HAL_GetTick();
  system_stable = 1;
}
static void Refresh_Watchdog(void)
{
  uint32_t current_time = HAL_GetTick();
  if ((current_time - watchdog_last_refresh) > 100) {  
    watchdog_last_refresh = current_time;
  }
}
static void Force_System_Reset(void)
{
  NVIC_SystemReset();
}
__attribute__((unused)) static void Disable_Watchdog_Temporarily(void)
{
  watchdog_timeout_ms = 15000;  
  Refresh_Watchdog();           
}
static uint8_t Build_Response_With_PA0(uint8_t base_response)
{
  Read_PA0_Digital();
  uint8_t response_lower = base_response & 0x0F;  
  uint8_t digital_mask = pa0_digital_state ? 0xF0 : 0x00;
  return digital_mask | response_lower;
}
static uint32_t Get_Device_UID(void)
{
  uint32_t uid_word0 = *(volatile uint32_t*)(UID_BASE_ADDRESS + 0);
  uint32_t uid_word1 = *(volatile uint32_t*)(UID_BASE_ADDRESS + 4);
  uint32_t uid_word2 = *(volatile uint32_t*)(UID_BASE_ADDRESS + 8);
  if ((uid_word0 == 0 && uid_word1 == 0 && uid_word2 == 0) ||
      (uid_word0 == 0xFFFFFFFF && uid_word1 == 0xFFFFFFFF && uid_word2 == 0xFFFFFFFF)) {
    uid_word0 = *(volatile uint32_t*)(UID_ALT_ADDRESS + 0);
    uid_word1 = *(volatile uint32_t*)(UID_ALT_ADDRESS + 4);
    uid_word2 = *(volatile uint32_t*)(UID_ALT_ADDRESS + 8);
  }
  if ((uid_word0 == 0 && uid_word1 == 0 && uid_word2 == 0) ||
      (uid_word0 == 0xFFFFFFFF && uid_word1 == 0xFFFFFFFF && uid_word2 == 0xFFFFFFFF)) {
    uid_word0 = (uint32_t)&uid_word0;           
    uid_word1 = HAL_GetTick();                  
    uid_word2 = ((uint32_t)&I2cHandle) ^ 0x12345678;  
  }
  uint32_t combined_uid = uid_word0 ^ uid_word1 ^ uid_word2;
  combined_uid = (combined_uid << 13) | (combined_uid >> 19);
  if (combined_uid == 0) {
    combined_uid = 0xDEADBEEF;
  }
  uint8_t test_addr = Calculate_I2C_Address_Internal(combined_uid);
  if (test_addr == 0x51) {
    combined_uid ^= 0xAAAA5555;  
  }
  return combined_uid;
}
static uint8_t Calculate_I2C_Address_Internal(uint32_t uid)
{
  uint32_t hash = uid;
  hash ^= (hash >> 16);
  hash ^= (hash >> 8);
  uint8_t addr = (uint8_t)(hash % (I2C_ADDR_MAX - I2C_ADDR_MIN + 1)) + I2C_ADDR_MIN;
  if (addr == 0x00 || addr == 0x01 || addr == 0x02 || addr == 0x03 ||
      addr == 0x04 || addr == 0x05 || addr == 0x06 || addr == 0x07 ||
      addr >= 0x78) {
    addr = 0x42;
  }
  return addr;
}
static uint8_t Calculate_I2C_Address(uint32_t uid)
{
  if (uid == 0 || uid == 0xFFFFFFFF) {
    return 0x42;
  }
  uint32_t hash = uid;
  hash ^= (hash >> 16);  
  hash ^= (hash >> 8);   
  uint8_t addr = (uint8_t)(hash % (I2C_ADDR_MAX - I2C_ADDR_MIN + 1)) + I2C_ADDR_MIN;
  if (addr == 0x00 || addr == 0x01 || addr == 0x02 || addr == 0x03 ||
      addr == 0x04 || addr == 0x05 || addr == 0x06 || addr == 0x07 ||
      addr >= 0x78) {
    addr = 0x42; 
  }
  if (addr == 0x51) {
    addr = 0x43;
  }
  return addr;
}
static void Disable_PF2_NRST_Function(void)
{
  return;
}
static void Init_PA2_PB5_Relay(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  
  GPIO_InitStruct.Pin = GPIO_PIN_5;              
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;    
  GPIO_InitStruct.Pull = GPIO_NOPULL;            
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
static void Pulse_Relays(void)
{
  pulse_pending = 1;
}
static void Process_Relay_Pulse(void)
{
  if (pulse_pending) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
    pulse_start_time = HAL_GetTick();
    pulse_active = 1;
    pulse_pending = 0;
  }
  if (pulse_active) {
    if ((HAL_GetTick() - pulse_start_time) >= 25) {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
      pulse_active = 0;
    }
  }
}
/*
static uint8_t Read_PB0_Digital(void)
{
  GPIO_PinState pin_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
  return (pin_state == GPIO_PIN_SET) ? 1 : 0;
}
*/
static uint8_t Check_PF2_NRST_Status(void)
{
  uint32_t user_reg = *(volatile uint32_t*)(0x1FFF0800);  
  if ((user_reg & (1U << 23)) == 0) {
    return 1;  
  } else {
    return 0;  
  }
}
static void Process_I2C_Command(void)
{
  last_command_time = HAL_GetTick();
  uint8_t response_code = RESP_CMD_UNKNOWN;
  if (flash_save_flag == 1)  
  {
    Save_DATA_To_Flash(aRxBuffer[0]);
    flash_save_flag = 0;
    response_code = RESPONSE_DATA;
    aTxBuffer[0] = Build_Response_With_PA0(response_code);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
    return;
  }
  else if (flash_save_flag == 2)  
  {
    uint8_t new_i2c_addr = aRxBuffer[0];
    if (new_i2c_addr >= I2C_ADDR_MIN && new_i2c_addr <= I2C_ADDR_MAX) {
      Set_Custom_I2C_Address(new_i2c_addr);
      response_code = RESP_I2C_ADDR_SET;
    } else {
      response_code = RESP_CMD_UNKNOWN;  
    }
    flash_save_flag = 0;
    aTxBuffer[0] = Build_Response_With_PA0(response_code);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
    return;
  }
  switch (aRxBuffer[0])
  {
  case SAVE_DATA:
    flash_save_flag = 1;  
    response_code = RESPONSE_DATA;  
    break;
  case READ_DATA:
  {
    uint16_t data = Read_DATA_From_Flash();
    aTxBuffer[0] = (uint8_t)(data & 0xFF);  
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
    return;
  }
  case CMD_SET_I2C_ADDR:
    flash_save_flag = 2;  
    response_code = RESP_I2C_ADDR_SET;  
    break;
  case CMD_RESET_FACTORY:
    Reset_To_Factory_I2C();
    response_code = RESP_FACTORY_RESET;
    break;
  case CMD_GET_I2C_STATUS:
    if (use_flash_i2c) {
      response_code = RESP_I2C_FROM_FLASH;
    } else {
      response_code = RESP_I2C_FROM_UID;
    }
    break;
  case CMD_RELAY_OFF:
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);  
    response_code = RESP_RELAY_OFF;
    break;
  case CMD_RELAY_ON:
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);    
    response_code = RESP_RELAY_ON;
    break;
  case CMD_RELAY_TOGGLE:
    if (!pulse_active && !pulse_pending) {
      Pulse_Relays();  
      response_code = RESP_RELAY_TOGGLE;
    } else {
      response_code = RESP_CMD_UNKNOWN;  
    }
    break;
  case CMD_RED:
    NEO_fillAll(255, 0, 0);  // Rojo
    NEO_update();
    HAL_Delay(1);  // Esperar a que termine transmisión SPI
    response_code = RESP_NEO_RED;
    system_stable = 0;  
    break;
  case CMD_GREEN:
    NEO_fillAll(0, 255, 0);  // Verde
    NEO_update();
    HAL_Delay(1);  // Esperar a que termine transmisión SPI
    Refresh_Watchdog();
    response_code = RESP_NEO_GREEN;
    break;
  case CMD_BLUE:
    NEO_fillAll(0, 0, 255);  // Azul
    NEO_update();
    HAL_Delay(1);  // Esperar a que termine transmisión SPI
    response_code = RESP_NEO_BLUE;
    break;
  case CMD_OFF:
    /* Send ACK immediately to free the I2C bus, then perform potentially
       disruptive operations (NeoPixel clear + PWM disable). This prevents
       holding the bus while touching timers/GPIO which could block the
       master or other devices. */
    response_code = RESP_NEO_OFF;
    aTxBuffer[0] = Build_Response_With_PA0(response_code);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);

    NEO_clearAll();  // Apagar NeoPixels (ya incluye NEO_update)
    HAL_Delay(1);  // Esperar a que termine transmisión SPI
    Refresh_Watchdog();
    system_stable = 1;
    PWM_Disable();  // Apagar PWM también
    return;
  case CMD_WHITE:
    NEO_fillAll(255, 255, 255);  // Blanco
    NEO_update();
    HAL_Delay(1);  // Esperar a que termine transmisión SPI
    response_code = RESP_NEO_OFF;  // Usar mismo código de respuesta
    break;
  case CMD_PWM_OFF:
    /* Acknowledge first, then disable PWM to avoid blocking I2C */
    response_code = RESP_NEO_OFF;
    aTxBuffer[0] = Build_Response_With_PA0(response_code);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);

    HAL_Delay(2);  // Pequeño delay para liberar bus I2C
    PWM_Disable();
    HAL_Delay(5);  // Delay adicional después de deshabilitar PWM
    Refresh_Watchdog();
    return;
  case CMD_PWM_25:
    /* Ack first */
    response_code = RESP_NEO_RED;
    aTxBuffer[0] = Build_Response_With_PA0(response_code);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);

    HAL_Delay(2);  // Liberar bus I2C
    PWM_Set_Frequency(200);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    HAL_Delay(2);  // Estabilizar después de enable
    Refresh_Watchdog();
    return;
  case CMD_PWM_50:
    response_code = RESP_NEO_GREEN;
    aTxBuffer[0] = Build_Response_With_PA0(response_code);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);

    HAL_Delay(2);
    PWM_Set_Frequency(500);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    HAL_Delay(2);
    Refresh_Watchdog();
    return;
  case CMD_PWM_75:
    response_code = RESP_NEO_BLUE;
    aTxBuffer[0] = Build_Response_With_PA0(response_code);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);

    HAL_Delay(2);
    PWM_Set_Frequency(1000);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    HAL_Delay(2);
    Refresh_Watchdog();
    return;
  case CMD_PWM_100:
    response_code = RESP_NEO_OFF;
    aTxBuffer[0] = Build_Response_With_PA0(response_code);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);

    HAL_Delay(2);
    PWM_Set_Frequency(2000);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    HAL_Delay(2);
    Refresh_Watchdog();
    return;
  /*
  // Comandos deshabilitados - Causan problemas de estabilidad I2C
  case CMD_TONE_DO:
    PWM_Set_Frequency(261);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_RED;
    break;
  case CMD_TONE_RE:
    PWM_Set_Frequency(294);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_RED;
    break;
  case CMD_TONE_MI:
    PWM_Set_Frequency(330);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_RED;
    break;
  case CMD_TONE_FA:
    PWM_Set_Frequency(349);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_RED;
    break;
  case CMD_TONE_SOL:
    PWM_Set_Frequency(392);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_GREEN;
    break;
  case CMD_TONE_LA:
    PWM_Set_Frequency(440);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_GREEN;
    break;
  case CMD_TONE_SI:
    PWM_Set_Frequency(494);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_GREEN;
    break;
  case CMD_SUCCESS:
    PWM_Set_Frequency(800);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_BLUE;
    break;
  case CMD_OK:
    PWM_Set_Frequency(1000);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_BLUE;
    break;
  case CMD_WARNING:
    PWM_Set_Frequency(1200);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_BLUE;
    break;
  case CMD_ALERT:
    PWM_Set_Frequency(1500);
    PWM_Set_Duty_Cycle(50);
    PWM_Enable();
    Refresh_Watchdog();
    response_code = RESP_NEO_BLUE;
    break;
  */
  case CMD_PA4_DIGITAL:
    if ((HAL_GetTick() - pa0_last_read) > 100 || pa0_initialized == 0) {
      Read_PA0_Digital();
    }
    response_code = RESP_PA4_DIGITAL;
    break;
  case CMD_PB0_DIGITAL:
    Init_PA0_Digital();  
    HAL_Delay(1);        
    Read_PA0_Digital();  
    response_code = Build_Response_With_PA0(RESP_PB0_DIGITAL);
    break;
  case CMD_DISABLE_NRST:
    Disable_PF2_NRST_Function();
    response_code = RESP_NRST_DISABLED;
    break;
  case CMD_CHECK_NRST:
  {
    uint8_t nrst_status = Check_PF2_NRST_Status();
    if (nrst_status == 1) {
      response_code = RESP_NRST_GPIO;     
    } else {
      response_code = RESP_NRST_ACTIVE;   
    }
    break;
  }
  case CMD_INIT_PB0:
    Init_PA2_PB5_Relay();
    response_code = RESP_PB0_INITIALIZED;
    break;
  case CMD_ADC_PA0_HSB:
  case CMD_ADC_PA0_LSB:
  case CMD_ADC_PA0_I2C:
    HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
    return;
  case CMD_RESET:
  {
    aTxBuffer[0] = Build_Response_With_PA0(RESP_RESET_OK);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    HAL_Delay(5);
    Force_System_Reset();
    break;
  }
  case CMD_WATCHDOG_RESET:
  {
    aTxBuffer[0] = Build_Response_With_PA0(RESP_WATCHDOG_OK);
    HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
    while(1) {
      HAL_Delay(100);
    }
    break;
  }
  default:
    response_code = RESP_CMD_UNKNOWN;
    break;
  }
  aTxBuffer[0] = Build_Response_With_PA0(response_code);
  HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 100);
  HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
}
int main(void)
{
  HAL_Init();
  APP_SystemClockConfig();
  
  // ═══════════════════════════════════════════════════════════
  //              INICIALIZACIÓN RÁPIDA - SIN SECUENCIA ARRANQUE
  // ═══════════════════════════════════════════════════════════
  
  // Inicializar periféricos básicos
  Init_PA2_PB5_Relay();
  PWM_Init_TIM1();
  NEO_init();
  Init_PF4_LED();
  
  // ═══════════════════════════════════════════════════════════
  //              CONFIGURACIÓN I2C Y SERVICIOS
  // ═══════════════════════════════════════════════════════════
  
  uint8_t dynamic_i2c_addr = Init_I2C_Address_Management();
  Init_i2c_pins();
  Init_PA0_Digital();              
  Init_Watchdog();                 
  __HAL_RCC_I2C_FORCE_RESET();
  __HAL_RCC_I2C_RELEASE_RESET();
  I2cHandle.Instance = I2C;
  I2cHandle.Init.ClockSpeed = I2C_SPEEDCLOCK;
  I2cHandle.Init.DutyCycle = I2C_DUTYCYCLE;
  I2cHandle.Init.OwnAddress1 = (dynamic_i2c_addr << 1); 
  I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  I2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&I2cHandle) != HAL_OK)
  {
    APP_ErrorHandler();
  }
  
  // MODO INTERRUPT: igual que i2c_slave_devlab
  HAL_NVIC_SetPriority(I2C1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(I2C1_IRQn);
  HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
  uint32_t last_i2c_check = HAL_GetTick();
  
  while (1)
  {
    Process_Relay_Pulse();
    if (i2c_rx_complete)
    {
      i2c_rx_complete = 0;
      Process_I2C_Command();
    }
    if ((HAL_GetTick() - last_i2c_check) > 1000)
    {
      last_i2c_check = HAL_GetTick();
      if (I2cHandle.State != HAL_I2C_STATE_LISTEN)
      {
        HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
      }
    }
  }
}
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  i2c_rx_complete = 1;
  error_count = 0;
}
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  i2c_tx_complete = 1;
  HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
}
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  error_count++;
  if (error_count >= MAX_I2C_ERRORS) {
    I2C_Recovery();
    error_count = 0;
  }
  HAL_I2C_Slave_Receive_IT(&I2cHandle, aRxBuffer, DATA_LENGTH);
}
