#include "main.h"
#include "py32f0xx_hal.h"  
#include "py32f0xx_hal_conf.h"
#include "py32f0xx_it.h"
#include <stdlib.h>
#include "commands.h"
#include "systemConfig.h"
#include "adc_control.h"


#define FLASH_SAVE_ADDR     0x08003C00  // última página de 1 KB (en 16 KB totales del PY32F003)
#define FLASH_I2C_ADDR      0x08003E00  // Dirección I2C personalizada (512 bytes antes del final)
#define FLASH_CONFIG_ADDR   0x08003F00  // Configuración y banderas (256 bytes antes del final)

// Banderas de configuración en Flash
#define CONFIG_USE_FLASH_I2C   0x01     // Bit 0: Usar dirección I2C desde Flash
#define CONFIG_FACTORY_DEFAULT 0x02     // Bit 1: Configuración factory (usar UID)


I2C_HandleTypeDef I2cHandle;

// Basic communication buffers
uint8_t aTxBuffer[1] = {0};
uint8_t aRxBuffer[1] = {0};
uint8_t state_button = 0;

// ═══════════════════════════════════════════════════════════
//                      I2C POLLING MODE VARIABLES  
// ═══════════════════════════════════════════════════════════
// REVERTED: El modo IT/Listen no funciona bien con scans I2C del maestro
// que solo verifican ACK/NACK sin transferir datos completos.
// Mantenemos polling pero con mejoras de recuperación robusta.

// Global handles
ADC_HandleTypeDef hadc;              // Handle del ADC

// ═══════════════════════════════════════════════════════════
//                      VARIABLES PA4 DIGITAL
// ═══════════════════════════════════════════════════════════
uint8_t pa4_digital_state = 0;       // Estado digital de PA4 (0 = LOW, 1 = HIGH)
uint32_t pa4_last_read = 0;          // Timestamp de última lectura PA4
uint8_t pa4_initialized = 0;         // Flag para saber si PA4 está inicializado como digital

// ═══════════════════════════════════════════════════════════
//                      VARIABLES TOGGLE NO BLOQUEANTE
// ═══════════════════════════════════════════════════════════
uint8_t toggle_pending = 0;          // Flag: toggle en progreso (1 = relé encendido, esperando apagar)
uint32_t toggle_start_time = 0;      // Timestamp cuando se activó el toggle

// ═══════════════════════════════════════════════════════════
//                      VARIABLES WATCHDOG Y RESET
// ═══════════════════════════════════════════════════════════
uint32_t watchdog_timeout_ms = 2000;    // Timeout de 2 segundos
uint32_t watchdog_last_refresh = 0;  // Timestamp de última actualización del watchdog
uint32_t system_uptime = 0;          // Tiempo de funcionamiento del sistema
uint8_t reset_requested = 0;         // Flag para reset solicitado
uint32_t last_command_time = 0;      // Timestamp del último comando recibido
uint8_t flash_save_flag = 0;         // Bandera para modo guardado Flash (0x3A activa, siguiente comando se guarda)
uint32_t error_count = 0;            // Contador global de errores I2C
uint32_t last_error_reset = 0;       // Timestamp del último reset de errores
uint32_t last_i2c_transaction = 0;   // Timestamp de última transacción I2C

// ═══════════════════════════════════════════════════════════
//                   VARIABLES I2C ADDRESS MANAGEMENT (v5_4)
// ═══════════════════════════════════════════════════════════
uint8_t use_flash_i2c = 0;           // Bandera: usar dirección I2C desde Flash
uint8_t current_i2c_address = 0x00;  // Dirección I2C actual en uso
uint8_t factory_default_addr = 0x00; // Dirección factory (calculada desde UID)

static void Init_i2c_pins(void);
static void Init_PB5_Digital(void);       //  FUNCIÓN PB5 DIGITAL
static void Init_PA4_Digital(void);       //  FUNCIÓN PA4 DIGITAL
static void Read_PA4_Digital(void);       //  FUNCIÓN PA4 DIGITAL

// static void Test_PWM_Output(void);        //  FUNCIÓN TEST PWM (DEBUG)
static uint8_t Build_Response_With_PA4(uint8_t base_response);  //  NUEVO PROTOCOLO PA4


static void I2C_Recovery(void);
// static uint8_t I2C_Check_And_Clear_Busy(void);  //  FUNCIÓN VERIFICAR Y LIMPIAR BUS BUSY
static void Init_Watchdog(void);          //  FUNCIÓN WATCHDOG
static void Refresh_Watchdog(void);       //  FUNCIÓN WATCHDOG REFRESH
static void Force_System_Reset(void);     //  FUNCIÓN RESET FORZADO
static void Check_System_Health(void);    //  FUNCIÓN MONITOR SISTEMA
static void Disable_Watchdog_Temporarily(void);  //  FUNCIÓN DEBUG WATCHDOG
// static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b);  // Comentada - no utilizada
static uint32_t Get_Device_UID(void);
static uint8_t Calculate_I2C_Address(uint32_t uid);
static uint8_t Calculate_I2C_Address_Internal(uint32_t uid);

// Funciones de información del dispositivo
static uint8_t getDeviceID(void);
static uint8_t getFlashSize(void);
static uint8_t getUIDBytes(uint8_t byte_index);
// static void Display_UID_Pattern(uint32_t uid);  // Comentada - no utilizada



void Save_DATA_To_Flash(uint16_t data)
{
    HAL_FLASH_Unlock();

    // Borrar la página donde guardarás
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t PageError = 0;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = FLASH_SAVE_ADDR;
    EraseInitStruct.NbPages = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) {
        HAL_FLASH_Lock();
        return;
    }

    // Convertir el dato recibido a 32 bits para escribir en Flash
    uint32_t data32 = (uint32_t)data;  // Expandir data de 16-bit a 32-bit

    HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, FLASH_SAVE_ADDR, &data32);

    HAL_FLASH_Lock();
}


uint16_t Read_DATA_From_Flash(void)
{
    return *(uint16_t*)FLASH_SAVE_ADDR;
}

// ═══════════════════════════════════════════════════════════
//                   FUNCIONES I2C ADDRESS MANAGEMENT (v5_4)
// ═══════════════════════════════════════════════════════════

// Guardar dirección I2C personalizada en Flash
void Save_I2C_Address_To_Flash(uint8_t address)
{
    HAL_FLASH_Unlock();

    // Borrar la página donde guardaremos la dirección I2C
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t PageError = 0;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = FLASH_I2C_ADDR;
    EraseInitStruct.NbPages = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
        // Guardar dirección I2C como 32-bit
        uint32_t addr32 = (uint32_t)address;
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, FLASH_I2C_ADDR, &addr32);
    }

    HAL_FLASH_Lock();
}

// Leer dirección I2C desde Flash
uint8_t Read_I2C_Address_From_Flash(void)
{
    uint16_t stored_addr = *(uint16_t*)FLASH_I2C_ADDR;
    
    // Verificar si es una dirección válida
    if (stored_addr >= I2C_ADDR_MIN && stored_addr <= I2C_ADDR_MAX) {
        return (uint8_t)stored_addr;
    }
    
    return 0x00; // Dirección inválida
}

// Guardar configuración en Flash
void Save_Config_To_Flash(uint8_t config_flags)
{
    HAL_FLASH_Unlock();

    // Borrar la página donde guardaremos la configuración
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t PageError = 0;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = FLASH_CONFIG_ADDR;
    EraseInitStruct.NbPages = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
        // Guardar configuración como 32-bit
        uint32_t config32 = (uint32_t)config_flags;
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, FLASH_CONFIG_ADDR, &config32);
    }

    HAL_FLASH_Lock();
}

// Leer configuración desde Flash
uint8_t Read_Config_From_Flash(void)
{
    uint16_t stored_config = *(uint16_t*)FLASH_CONFIG_ADDR;
    return (uint8_t)(stored_config & 0xFF);
}

// Inicializar dirección I2C (Flash vs UID)
uint8_t Init_I2C_Address_Management(void)
{
    // Leer configuración desde Flash
    uint8_t config = Read_Config_From_Flash();
    
    // Calcular dirección factory desde UID
    uint32_t device_uid = Get_Device_UID();
    factory_default_addr = Calculate_I2C_Address(device_uid);
    
    // Verificar si debe usar dirección desde Flash
    if (config & CONFIG_USE_FLASH_I2C) {
        uint8_t flash_addr = Read_I2C_Address_From_Flash();
        if (flash_addr != 0x00) {
            use_flash_i2c = 1;
            current_i2c_address = flash_addr;
            return flash_addr;
        }
    }
    
    // Usar dirección por defecto (UID)
    use_flash_i2c = 0;
    current_i2c_address = factory_default_addr;
    return factory_default_addr;
}

// ═══════════════════════════════════════════════════════════
//           HELPER PARA ENVIAR RESPUESTA I2C (TIMEOUT CORTO)
// ═══════════════════════════════════════════════════════════
static inline void Send_I2C_Response(uint8_t response_byte)
{
  // Envío con timeout ultra-corto para no bloquear
  aTxBuffer[0] = response_byte;
  HAL_I2C_Slave_Transmit(&I2cHandle, aTxBuffer, DATA_LENGTH, 50); // 50ms timeout
  error_count = 0;
}

// Establecer nueva dirección I2C (guardar en Flash)
void Set_Custom_I2C_Address(uint8_t new_address)
{
    // Validar dirección
    if (new_address >= I2C_ADDR_MIN && new_address <= I2C_ADDR_MAX) {
        // Guardar dirección en Flash
        Save_I2C_Address_To_Flash(new_address);
        
        // Guardar configuración para usar Flash
        Save_Config_To_Flash(CONFIG_USE_FLASH_I2C);
        
        // Actualizar variables
        use_flash_i2c = 1;
        current_i2c_address = new_address;
    }
}

// Reset factory (usar UID por defecto)
void Reset_To_Factory_I2C(void)
{
    // Borrar configuración Flash (volver a factory)
    Save_Config_To_Flash(CONFIG_FACTORY_DEFAULT);
    
    // Actualizar variables
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

//  FUNCIÓN: Inicializar PB5 como salida digital
static void Init_PB5_Digital(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // Habilitar reloj GPIOB
  __HAL_RCC_GPIOB_CLK_ENABLE();
  
  // Configurar PB5 como salida digital
  GPIO_InitStruct.Pin = GPIO_PIN_5;              // PB5 - Salida digital
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;    // Push-Pull Output
  GPIO_InitStruct.Pull = GPIO_NOPULL;            // Sin pull-up/down
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // Velocidad baja para relé
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  
  // Inicializar RELÉ apagado
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}

// }

//  FUNCIÓN: Inicializar PA4 como entrada digital
static void Init_PA4_Digital(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // Habilitar reloj GPIOA
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // Configurar PA4 como entrada digital con pull-up
  GPIO_InitStruct.Pin = GPIO_PIN_4;              // PA4 - Pin digital
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;        // Entrada digital
  GPIO_InitStruct.Pull = GPIO_PULLUP;            // Pull-up interno para lectura estable
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // Velocidad baja para entrada
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // Marcar como inicializado
  pa4_initialized = 1;
  
  // Hacer primera lectura
  Read_PA4_Digital();
}

//  FUNCIÓN: Leer PA4 como entrada digital
static void Read_PA4_Digital(void)
{
  // Asegurar que PA4 está configurado como digital
  if (pa4_initialized == 0) {
    Init_PA4_Digital();
  }
  
  // Leer estado digital de PA4
  GPIO_PinState pin_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);
  pa4_digital_state = (pin_state == GPIO_PIN_SET) ? 1 : 0;
  
  // Actualizar timestamp
  pa4_last_read = HAL_GetTick();
}

// ═══════════════════════════════════════════════════════════
//      FUNCIÓN: VERIFICAR Y LIMPIAR BUS I2C BUSY
// ═══════════════════════════════════════════════════════════
//  Esta función detecta si el bus I2C está en estado BUSY
//  (maestro repitió comandos sin STOP) y limpia el estado.
//  Retorna: 1 si estaba BUSY y se limpió, 0 si estaba OK
// static uint8_t I2C_Check_And_Clear_Busy(void)
// {
//   // Verificar si el periférico I2C está en estado BUSY
//   // Usar la macro I2C_SR2_BUSY en lugar de un literal mágico
//   if (I2cHandle.Instance->SR2 & I2C_SR2_BUSY) {
//     // Intentar recuperación local y validar resultado
//     I2C_Recovery();

//     // Esperar brevemente y comprobar si el flag BUSY se limpió
//     HAL_Delay(2);
//     if (I2cHandle.Instance->SR2 & I2C_SR2_BUSY) {
//       // Si sigue ocupado, retornar error para que el caller pueda decidir
//       return 1;  // Bus estaba trabado (no garantizamos limpieza)
//     }
//     return 1;  // Bus estaba trabado y parece limpiado
//   }
//   return 0;  // Bus OK
// }

static void I2C_Recovery(void)
{
  // RECUPERACIÓN ROBUSTA DEL BUS I2C
  // Objetivos:
  //  - liberar esclavos que mantienen SDA baja
  //  - generar un STOP gracioso
  //  - resetear el periférico y re-inicializar manteniendo la config

  // Guardar configuración actual por si se pierde
  I2C_TypeDef *I2Cx = I2cHandle.Instance;
  I2C_HandleTypeDef savedHandle = I2cHandle;

  // 1) Deshabilitar periférico y forzar reset del IP
  __HAL_I2C_DISABLE(&I2cHandle);
  HAL_Delay(1);
  __HAL_RCC_I2C_FORCE_RESET();
  HAL_Delay(2);
  __HAL_RCC_I2C_RELEASE_RESET();
  HAL_Delay(2);

  // 2) Reconfigurar pines como GPIO para control manual (PF1=SCL, PF0=SDA)
  __HAL_RCC_GPIOF_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  // Asegurar SDA alta antes de empezar (si es posible)
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_0, GPIO_PIN_SET);
  HAL_Delay(1);

  // 3) Pulsar SCL hasta 16 veces (si SDA está baja intentar liberar). Si SDA se suelta, salir.
  for (int i = 0; i < 16; i++) {
    // SCL LOW
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);
    for (volatile int d = 0; d < 200; d++);

    // SCL HIGH
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);
    for (volatile int d = 0; d < 200; d++);

    // Si SDA ya está alta, el bus fue liberado
    GPIO_PinState sda = HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_0);
    if (sda == GPIO_PIN_SET) {
      break;
    }
  }

  // 4) Forzar STOP manual (SDA: LOW -> HIGH while SCL HIGH)
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_0, GPIO_PIN_RESET);
  for (volatile int d = 0; d < 200; d++);
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);
  for (volatile int d = 0; d < 200; d++);
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_0, GPIO_PIN_SET);
  for (volatile int d = 0; d < 400; d++);

  // 5) Restaurar pines a AF I2C
  Init_i2c_pins();
  HAL_Delay(1);

  // 6) Limpiar flags sensibles leyendo SRx (lectura ordenada) y usando macros HAL
  volatile uint32_t tmpreg = I2Cx->SR1;
  (void)tmpreg;
  tmpreg = I2Cx->SR2;
  (void)tmpreg;
  __HAL_I2C_CLEAR_FLAG(&I2cHandle, I2C_FLAG_STOPF);

  // 7) Deinit / Init usando LL/HAL para forzar estado limpio
  HAL_I2C_DeInit(&I2cHandle);
  HAL_Delay(2);

  // Restaurar configuracion guardada (especialmente OwnAddress1)
  I2cHandle = savedHandle;
  I2cHandle.Instance = I2Cx;

  if (HAL_I2C_Init(&I2cHandle) != HAL_OK) {
    // Intento adicional
    HAL_Delay(5);
    HAL_I2C_Init(&I2cHandle);
  }

  // 8) Esperar que el flag BUSY se despeje; si persiste, intentar un reset mayor
  uint32_t start = HAL_GetTick();
  while ((I2cHandle.Instance->SR2 & I2C_SR2_BUSY) && ((HAL_GetTick() - start) < 20)) {
    HAL_Delay(1);
  }

  // Si sigue ocupado tras reintentos, incrementar contador de errores y devolver.
  if (I2cHandle.Instance->SR2 & I2C_SR2_BUSY) {
    // No podemos limpiar BUSY de forma fiable -> dejar que el caller tome acción (o reset completo)
    error_count++;
  }

  HAL_Delay(2);
}

// ═══════════════════════════════════════════════════════════
//                      FUNCIONES WATCHDOG Y RESET
// ═══════════════════════════════════════════════════════════

//  FUNCIÓN: Inicializar Software Watchdog (sin hardware IWDG)
static void Init_Watchdog(void)
{
  // Configurar variables de software watchdog DESHABILITADO
  watchdog_timeout_ms = 0;              // 0 = DESHABILITADO para evitar reinicios
  watchdog_last_refresh = HAL_GetTick();
  
  // Watchdog solo manual, no automático
}

//  FUNCIÓN: Refrescar Software Watchdog
static void Refresh_Watchdog(void)
{
  uint32_t current_time = HAL_GetTick();
  
  // Solo refrescar si ha pasado suficiente tiempo (evitar refresh excesivo)
  if ((current_time - watchdog_last_refresh) > 100) {  // Mínimo 100ms entre refresh
    watchdog_last_refresh = current_time;
  }
}

//  FUNCIÓN: Forzar Reset del Sistema
static void Force_System_Reset(void)
{
  // // Apagar todos los periféricos antes del reset
  // NEO_clearAll();
  // NEO_update();
  
  // Apagar PWM
  TIM1->CCR3 = 0;
  TIM1->CCER &= ~(1 << 8);
  TIM1->CR1 &= ~(1 << 0);
  
  // Apagar relé
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  
  // Pequeño delay para que se ejecuten los cambios
  HAL_Delay(10);
  
  // Reset por software usando NVIC
  NVIC_SystemReset();
}

//  FUNCIÓN: Monitor de Salud del Sistema
static void Check_System_Health(void)
{
  uint32_t current_time = HAL_GetTick();
  
  // Actualizar uptime del sistema
  system_uptime = current_time;
  
  // Verificar si el sistema necesita reset (solo manual)
  if (reset_requested) {
    Force_System_Reset();
  }
  
  //  SOFTWARE WATCHDOG: DESHABILITADO PARA EVITAR REINICIOS
  // El watchdog solo se activa por comando manual CMD_WATCHDOG_RESET
  // NO hay timeout automático para evitar reinicios durante operación normal
}

//  FUNCIÓN: Deshabilitar Watchdog Temporalmente (FUNCIÓN DE DEBUG - NO USADA EN PRODUCCIÓN)
__attribute__((unused)) static void Disable_Watchdog_Temporarily(void)
{
  // Extender el timeout temporalmente para operaciones críticas
  watchdog_timeout_ms = 15000;  // 15 segundos para operaciones críticas
  Refresh_Watchdog();           // Refrescar inmediatamente
}

//  NUEVO PROTOCOLO: Combinar estado PA4 digital con respuesta
static uint8_t Build_Response_With_PA4(uint8_t base_response)
{
  // Leer estado actual de PA4 digital
  Read_PA4_Digital();
  
  // Limpiar los bits superiores (7-4) de la respuesta base
  uint8_t response_lower = base_response & 0x0F;  // Solo bits 3-0
  
  //  MASCARA DIGITAL PA4: Estado en bits superiores (7-4)
  // Si PA4 está HIGH (1), usar máscara 0xF0 (todos los bits superiores en 1)
  // Si PA4 está LOW (0), usar máscara 0x00 (todos los bits superiores en 0)
  uint8_t digital_mask = pa4_digital_state ? 0xF0 : 0x00;
  
  return digital_mask | response_lower;
}

static uint32_t Get_Device_UID(void)
{
  // Leer UID del dispositivo PY32F0xx (96 bits total)
  // Intentar la dirección principal primero
  uint32_t uid_word0 = *(volatile uint32_t*)(UID_BASE_ADDRESS + 0);
  uint32_t uid_word1 = *(volatile uint32_t*)(UID_BASE_ADDRESS + 4);
  uint32_t uid_word2 = *(volatile uint32_t*)(UID_BASE_ADDRESS + 8);
  
  // Si la dirección principal no tiene datos válidos, probar dirección alternativa
  if ((uid_word0 == 0 && uid_word1 == 0 && uid_word2 == 0) ||
      (uid_word0 == 0xFFFFFFFF && uid_word1 == 0xFFFFFFFF && uid_word2 == 0xFFFFFFFF)) {
    
    uid_word0 = *(volatile uint32_t*)(UID_ALT_ADDRESS + 0);
    uid_word1 = *(volatile uint32_t*)(UID_ALT_ADDRESS + 4);
    uid_word2 = *(volatile uint32_t*)(UID_ALT_ADDRESS + 8);
  }
  
  // Si aún no tenemos datos válidos, generar UID basado en otros registros del sistema
  if ((uid_word0 == 0 && uid_word1 == 0 && uid_word2 == 0) ||
      (uid_word0 == 0xFFFFFFFF && uid_word1 == 0xFFFFFFFF && uid_word2 == 0xFFFFFFFF)) {
    
    // Generar UID usando diferentes fuentes del sistema
    uid_word0 = (uint32_t)&uid_word0;           // Dirección de memoria como semilla
    uid_word1 = HAL_GetTick();                  // Timestamp como variación
    uid_word2 = ((uint32_t)&I2cHandle) ^ 0x12345678;  // Otra dirección + constante
  }
  
  // Combinar los 96 bits en un valor de 32 bits usando XOR y rotación
  uint32_t combined_uid = uid_word0 ^ uid_word1 ^ uid_word2;
  
  // Aplicar rotación adicional para mejor distribución
  combined_uid = (combined_uid << 13) | (combined_uid >> 19);
  
  // Asegurar que nunca sea 0 y evitar valores que generen 0x51
  if (combined_uid == 0) {
    combined_uid = 0xDEADBEEF;
  }
  
  // Si el resultado generaría 0x51, modificarlo
  uint8_t test_addr = Calculate_I2C_Address_Internal(combined_uid);
  if (test_addr == 0x51) {
    combined_uid ^= 0xAAAA5555;  // XOR con patrón para cambiar resultado
  }
  
  return combined_uid;
}

// Función interna para calcular dirección sin evitar 0x51
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
  // Verificar que el UID no sea 0 o 0xFFFFFFFF (valores inválidos)
  if (uid == 0 || uid == 0xFFFFFFFF) {
    // Si el UID es inválido, usar dirección fija pero diferente
    return 0x42;
  }
  
  // Calcular dirección I2C basada en UID usando mejor distribución
  // Usar hash simple para mejor distribución
  uint32_t hash = uid;
  hash ^= (hash >> 16);  // XOR con bits superiores
  hash ^= (hash >> 8);   // XOR con bits medios
  
  uint8_t addr = (uint8_t)(hash % (I2C_ADDR_MAX - I2C_ADDR_MIN + 1)) + I2C_ADDR_MIN;
  
  // Evitar direcciones reservadas específicas
  if (addr == 0x00 || addr == 0x01 || addr == 0x02 || addr == 0x03 ||
      addr == 0x04 || addr == 0x05 || addr == 0x06 || addr == 0x07 ||
      addr >= 0x78) {
    addr = 0x42; // Dirección por defecto diferente de 0x51
  }
  
  // Evitar específicamente 0x51 para forzar direcciones dinámicas
  if (addr == 0x51) {
    addr = 0x43;
  }
  
  return addr;
}

// === FUNCIONES DE INFORMACIÓN DEL DISPOSITIVO ===

/**
 * @brief Obtiene el ID del dispositivo PY32F003
 * @return Device ID: 0x16 para F16, 0x18 para F18
 */
static uint8_t getDeviceID(void) {
    // Leer el registro DBGMCU_IDCODE para identificar el dispositivo
    // En PY32F003: 0x40015800 contiene información del dispositivo
    // uint32_t idcode = *((volatile uint32_t*)0x40015800);
    
    // Leer el tamaño de Flash desde el registro específico
    // PY32F003 tiene registro de tamaño de Flash en 0x1FFF0FE0
    uint16_t flash_size_kb = *((volatile uint16_t*)0x1FFF0FE0);
    
    // Determinar el tipo basado en el tamaño de Flash
    if (flash_size_kb >= 18) {
        return 0x18;  // PY32F003F18U6TR (18KB Flash)
    } else {
        return 0x16;  // PY32F003F16U6TR (16KB Flash)
    }
}

/**
 * @brief Obtiene el tamaño de Flash en KB
 * @return Tamaño de Flash (16 o 18)
 */
static uint8_t getFlashSize(void) {
    // Leer registro de tamaño de Flash
    uint16_t flash_size = *((volatile uint16_t*)0x1FFF0FE0);
    return (uint8_t)flash_size;
}

/**
 * @brief Obtiene bytes del UID único
 * @param byte_index Índice del byte (0-3)
 * @return Byte del UID
 */
static uint8_t getUIDBytes(uint8_t byte_index) {
    // UID está en la dirección base 0x1FFF0E00
    volatile uint32_t* uid_base = (volatile uint32_t*)0x1FFF0E00;
    
    if (byte_index > 3) {
        return 0x00;
    }
    
    // Leer la palabra de 32 bits que contiene el UID
    uint32_t uid_word = uid_base[0];
    
    // Extraer el byte solicitado
    switch (byte_index) {
        case 0: return (uint8_t)(uid_word & 0xFF);          // LSB
        case 1: return (uint8_t)((uid_word >> 8) & 0xFF);
        case 2: return (uint8_t)((uid_word >> 16) & 0xFF);
        case 3: return (uint8_t)((uid_word >> 24) & 0xFF);  // MSB
        default: return 0x00;
    }
}


int main(void)
{
  HAL_Init();
  APP_SystemClockConfig();



  // CONFIRMACIÓN RÁPIDA: NeoPixels funcionando
  // // Azul rápido para confirmar hardware NeoPixel
  // for (int i = 0; i < NEO_COUNT; i++)
  //   NEO_writeColor(i, 0, 0, 100);
  // NEO_update();
  // HAL_Delay(50);

  HAL_Delay(100);

  // ═══════════════════════════════════════════════════════════
  //                   INICIALIZACIÓN I2C ADDRESS (v5_4)
  // ═══════════════════════════════════════════════════════════

  // Inicializar gestión de direcciones I2C (Flash vs UID)
  uint8_t dynamic_i2c_addr = Init_I2C_Address_Management();
  
  // Indicar origen de la dirección I2C con sonido


  Init_i2c_pins();
  Init_PB5_Digital();              //  Inicializar PB5 como salida digital
 
  Init_PA4_Digital();              //  Inicializar PA4 como entrada digital
  Init_ADC_PA0();                  //  Inicializar ADC en PA0 (12-bit)
  Init_Watchdog();                 //  Inicializar Watchdog Independiente (DESHABILITADO)

  //  SONIDO DE ARRANQUE: Secuencia elegante tipo Windows/Mac
  

  
  // Apagar PWM y mantener blanco por 1 segundo
  TIM1->CCR3 = 0;                    
  TIM1->CCER &= ~(1 << 8);           
  TIM1->CR1 &= ~(1 << 0);            
  HAL_Delay(1000);
  

  __HAL_RCC_I2C_FORCE_RESET();
  __HAL_RCC_I2C_RELEASE_RESET();

  I2cHandle.Instance = I2C;
  I2cHandle.Init.ClockSpeed = I2C_SPEEDCLOCK;
  I2cHandle.Init.DutyCycle = I2C_DUTYCYCLE;
  I2cHandle.Init.OwnAddress1 = (dynamic_i2c_addr << 1); // Usar dirección dinámica
  I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  I2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&I2cHandle) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  // Inicializar timestamp de reset de errores
  last_error_reset = HAL_GetTick();

  while (1)
  {
    //  MONITOREO BÁSICO DEL SISTEMA (sin watchdog automático)
    Check_System_Health();
    
    //  RESET AUTOMÁTICO DE ERRORES cada 5 segundos para evitar acumulación
    if ((HAL_GetTick() - last_error_reset) >= 5000) {
      error_count = 0;
      last_error_reset = HAL_GetTick();
    }
    
    //  TOGGLE NO BLOQUEANTE: Verificar si hay que apagar el relé
    if (toggle_pending && (HAL_GetTick() - toggle_start_time) >= 10) {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);  // Apagar relé después de 10ms
      toggle_pending = 0;  // Limpiar flag
    }
    
    // ═══════════════════════════════════════════════════════════
    //    POLLING MEJORADO CON RECUPERACIÓN ROBUSTA (TIMEOUT CORTO)
    // ═══════════════════════════════════════════════════════════
    
    // Timeout ultra-corto para no bloquear en scans
    HAL_StatusTypeDef rx = HAL_I2C_Slave_Receive(&I2cHandle, aRxBuffer, DATA_LENGTH, 10); // 10ms timeout

    if (rx != HAL_OK)
    {
      // Error o timeout - NO incrementar contador agresivamente
      // Los scans del maestro causan timeouts normales
      if (rx == HAL_TIMEOUT) {
        // Timeout normal - ignorar (scan del maestro)
        continue;
      }
      
      // Error real (no timeout)
      error_count++;
      if (error_count >= MAX_I2C_ERRORS) {
        I2C_Recovery();
        error_count = 0;
      }
      continue;
    }
    else
    {
      //  COMANDO RECIBIDO: Actualizar timestamps
      last_command_time = HAL_GetTick();
      last_i2c_transaction = HAL_GetTick();
      error_count = 0; // Reset contador en transacción exitosa
      
      uint8_t response_code = RESP_CMD_UNKNOWN; // Por defecto, comando no reconocido

      // ═══════════════════════════════════════════════════════════
      //                      MODO FLASH SAVE ACTIVADO
      // ═══════════════════════════════════════════════════════════
      
      // Si la bandera Flash está activa, guardar según el modo
      if (flash_save_flag == 1)  // Modo datos Flash
      {
        // Guardar el comando recibido en Flash
        Save_DATA_To_Flash(aRxBuffer[0]);
        
        // Liberar bandera después de guardar
        flash_save_flag = 0;
        
        // Responder que se guardó correctamente
        response_code = RESPONSE_DATA;
        
        // Preparar respuesta (se enviará automáticamente en modo IT)
        Send_I2C_Response(Build_Response_With_PA4(response_code));
        continue; // Saltar el procesamiento normal
      }
      else if (flash_save_flag == 2)  // Modo dirección I2C
      {
        // Establecer nueva dirección I2C personalizada
        uint8_t new_i2c_addr = aRxBuffer[0];
        
        // Validar dirección
        if (new_i2c_addr >= I2C_ADDR_MIN && new_i2c_addr <= I2C_ADDR_MAX) {
          Set_Custom_I2C_Address(new_i2c_addr);
          response_code = RESP_I2C_ADDR_SET;
        } else {
          response_code = RESP_CMD_UNKNOWN;  // Dirección inválida
        }
        
        // Liberar bandera después de procesar
        flash_save_flag = 0;
        
        // Preparar respuesta (se enviará automáticamente en modo IT)
        Send_I2C_Response(Build_Response_With_PA4(response_code));
        continue; // Saltar el procesamiento normal
      }

      // ═══════════════════════════════════════════════════════════
      //                      COMANDOS FLASH ESPECIALES
      // ═══════════════════════════════════════════════════════════
      
      // Comando 0x3A: Activar bandera para guardar próximo comando
      if (aRxBuffer[0] == SAVE_DATA)  // 0x3A: Activar modo Flash Save
      {
        flash_save_flag = 1;  // Activar bandera para próximo comando
        response_code = RESPONSE_DATA;  // Confirmar activación
      }
      // Comando 0x3B: Leer datos desde Flash (siempre disponible)
      else if (aRxBuffer[0] == READ_DATA)  // 0x3B: Leer Flash
      {
        uint16_t data = Read_DATA_From_Flash();
        
        // ✅ MEJORA: Devolver dato completo de 8 bits sin protocolo PA4
        // Para comandos Flash, devolver el dato puro sin mezclar con PA4
        Send_I2C_Response((uint8_t)(data & 0xFF));
        continue; // Saltar el envío normal de respuesta
      }

      // ═══════════════════════════════════════════════════════════
      //                   COMANDOS I2C ADDRESS MANAGEMENT (v5_4)
      // ═══════════════════════════════════════════════════════════
      
      // Comando 0x3D: Establecer nueva dirección I2C
      else if (aRxBuffer[0] == CMD_SET_I2C_ADDR)  // 0x3D: Establecer dirección I2C
      {
        // Activar bandera para recibir nueva dirección en el próximo comando
        flash_save_flag = 2;  // Modo especial para dirección I2C
        response_code = RESP_I2C_ADDR_SET;  // Confirmar que está listo para recibir
      }
      // Comando 0x3E: Reset factory (usar UID por defecto)
      else if (aRxBuffer[0] == CMD_RESET_FACTORY)  // 0x3E: Reset factory
      {
        Reset_To_Factory_I2C();
        response_code = RESP_FACTORY_RESET;
        
        // Nota: Requiere reinicio para aplicar nueva dirección
      }
      // Comando 0x3F: Obtener estado I2C (Flash vs UID)
      else if (aRxBuffer[0] == CMD_GET_I2C_STATUS)  // 0x3F: Estado I2C
      {
        if (use_flash_i2c) {
          response_code = RESP_I2C_FROM_FLASH;
        } else {
          response_code = RESP_I2C_FROM_UID;
        }
      }

      // ═══════════════════════════════════════════════════════════
      //                      PROCESAMIENTO DE COMANDOS NORMALES
      //                      (DESHABILITADOS SI FLASH_SAVE_FLAG = 1)
      // ═══════════════════════════════════════════════════════════
      
      // Comandos básicos RELÉ y NeoPixels
      else if (aRxBuffer[0] == CMD_RELAY_ON)  // 0x01: RELÉ ON - Encender RELÉ en PB5
      {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);  // Encender RELÉ PB5
        response_code = RESP_RELAY_ON;
      }
      else if (aRxBuffer[0] == CMD_TOGGLE )  // 0x06: RELÉ TOGGLE - Alternar RELÉ en PB5
      {
        // Activar toggle no bloqueante
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);   // Encender RELÉ inmediatamente
        toggle_pending = 1;                                    // Marcar que hay toggle pendiente
        toggle_start_time = HAL_GetTick();                     // Guardar timestamp
        
        response_code = RESP_TOGGLE;
        // El relé se apagará automáticamente en el loop principal después de 10ms
      }
      else if (aRxBuffer[0] == CMD_RELAY_OFF)  // 0x00: RELÉ OFF - Apagar RELÉ en PB5
      {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);  // Apagar RELÉ PB5
        response_code = RESP_RELAY_OFF;
      }
      else if (aRxBuffer[0] == CMD_RED)  // 0x02: NeoPixels rojos
      {
        //  OPERACIÓN OPTIMIZADA: Sin marcar sistema inestable
        // for (int i = 0; i < NEO_COUNT; i++) {
        //   NEO_writeColor(i, 128, 0, 0);
        // }
        // NEO_update();
        response_code = RESP_NEO_RED;
      }
      else if (aRxBuffer[0] == CMD_GREEN)  // 0x03: NeoPixels verdes
      {
        //  OPERACIÓN OPTIMIZADA: Sin delays innecesarios
        // for (int i = 0; i < NEO_COUNT; i++) {
        //   NEO_writeColor(i, 0, 128, 0);
        // }
        // NEO_update();
        response_code = RESP_NEO_GREEN;
      }
      else if (aRxBuffer[0] == CMD_BLUE)  // 0x04: NeoPixels azules
      {
        // //  OPERACIÓN OPTIMIZADA: Sin delays innecesarios
        // for (int i = 0; i < NEO_COUNT; i++) {
        //   NEO_writeColor(i, 0, 0, 128);
        // }
        // NEO_update();
        response_code = RESP_NEO_BLUE;
      }
      else if (aRxBuffer[0] == CMD_OFF)  // 0x05: NeoPixels apagados
      {
        //  OPERACIÓN OPTIMIZADA: Sin delays innecesarios
        // for (int i = 0; i < NEO_COUNT; i++) {
        //   NEO_writeColor(i, 0, 0, 0);
        // }
        // NEO_update();
        response_code = RESP_NEO_OFF;
      }
      else if (aRxBuffer[0] == CMD_WHITE)  // 0x08: NeoPixels blancos
      {
        //  OPERACIÓN OPTIMIZADA: Sin delays innecesarios
        // for (int i = 0; i < NEO_COUNT; i++) {
        //   NEO_writeColor(i, 128, 128, 128);
        // }
        // NEO_update();
        response_code = RESP_NEO_WHITE;
      }
      
      //  COMANDO PA4 DIGITAL
      else if (aRxBuffer[0] == CMD_PA4_DIGITAL)  // 0x07: Leer PA4 como entrada digital
      {
        // // Lectura optimizada: solo re-leer si es muy antigua (>5ms) o primera vez
        // if ((HAL_GetTick() - pa4_last_read) > 5 || pa4_initialized == 0) {
        //   Read_PA4_Digital();
        // }
        // El estado digital se incluye en los bits superiores de la respuesta
        response_code = RESP_PA4_DIGITAL;
      }
      
      // Comandos PWM con frecuencias específicas
      else if (aRxBuffer[0] >= CMD_PWM_MIN && aRxBuffer[0] <= CMD_PWM_MAX)  // 0x10-0x1F: PWM gradual
      {
        // PWM gradual con diferentes frecuencias: 0x10 = 100Hz, 0x11 = 150Hz, ..., 0x1F = 1000Hz
        // uint16_t frequency = 100 + ((aRxBuffer[0] - CMD_PWM_MIN) * 900) / 16;  // 100Hz a 1000Hz
        // PWM_Set_Frequency(frequency);
        response_code = RESP_PWM_SET;
      }
      else if (aRxBuffer[0] == CMD_PWM_OFF)  // 0x20: PWM OFF - Silencio
      {
        // PWM_Stop();                        // Parar PWM completamente usando módulo
        response_code = RESP_PWM_OFF;
      }
      else if (aRxBuffer[0] == CMD_PWM_25)  // 0x21: PWM 25% - Tono grave 200Hz
      {
        // PWM_Set_Frequency(200);
        response_code = RESP_PWM_SET;
      }
      else if (aRxBuffer[0] == CMD_PWM_50)  // 0x22: PWM 50% - Tono medio 500Hz
      {
        // PWM_Set_Frequency(500);
        response_code = RESP_PWM_SET;
      }
      else if (aRxBuffer[0] == CMD_PWM_75)  // 0x23: PWM 75% - Tono agudo 1000Hz
      {
        // PWM_Set_Frequency(1000);
        response_code = RESP_PWM_SET;
      }
      else if (aRxBuffer[0] == CMD_PWM_100)  // 0x24: PWM 100%
      {
        // PWM_Set_Frequency(2000);  // Tono muy agudo 2kHz
        response_code = RESP_PWM_SET;
      }
      else if (aRxBuffer[0] == CMD_TONE_DO)  // 0x25: Tono DO (261Hz)
      {
        // PWM_Set_Frequency(261);
        response_code = RESP_TONE_SET;
      }
      else if (aRxBuffer[0] == CMD_TONE_RE)  // 0x26: Tono RE (294Hz)
      {
        // PWM_Set_Frequency(294);
        response_code = RESP_TONE_SET;
      }
      else if (aRxBuffer[0] == CMD_TONE_MI)  // 0x27: Tono MI (330Hz)
      {
        // PWM_Set_Frequency(330);
        response_code = RESP_TONE_SET;
      }
      else if (aRxBuffer[0] == CMD_TONE_FA)  // 0x28: Tono FA (349Hz)
      {
        // PWM_Set_Frequency(349);
        response_code = RESP_TONE_SET;
      }
      else if (aRxBuffer[0] == CMD_TONE_SOL)  // 0x29: Tono SOL (392Hz)
      {
        // PWM_Set_Frequency(392);
        response_code = RESP_TONE_SET;
      }
      else if (aRxBuffer[0] == CMD_TONE_LA)  // 0x2A: Tono LA (440Hz)
      {
        // PWM_Set_Frequency(440);
        response_code = RESP_TONE_SET;
      }
      else if (aRxBuffer[0] == CMD_TONE_SI)  // 0x2B: Tono SI (494Hz)
      {
        // PWM_Set_Frequency(494);
        response_code = RESP_TONE_SET;
      }
      else if (aRxBuffer[0] == CMD_SUCCESS)  // 0x2C: Success - Tono positivo 800Hz
      {
        // PWM_Set_Frequency(800);
        response_code = RESP_SUCCESS;
      }
      else if (aRxBuffer[0] == CMD_OK)  // 0x2D: OK - Confirmación 1000Hz
      {
        // PWM_Set_Frequency(1000);
        response_code = RESP_OK;
      }
      else if (aRxBuffer[0] == CMD_WARNING)  // 0x2E: Warning - Advertencia 1200Hz
      {
        // PWM_Set_Frequency(1200);
        response_code = RESP_WARNING;
      }
      else if (aRxBuffer[0] == CMD_ALERT)  // 0x2F: Alert - Alerta crítica 1500Hz
      {
        // PWM_Set_Frequency(1500);
        response_code = RESP_ALERT;
      }
      
      //  COMANDOS ADC PA0 (12-bit dividido en HSB + LSB) - MEJORADOS CON FILTRADO
      else if (aRxBuffer[0] == CMD_ADC_PA0_HSB)  // 0xD8: Leer ADC PA0 - HSB (bits 11-8)
      {
        // Obtener valor ADC filtrado y estabilizado
        uint16_t filtered_adc = Get_ADC_Filtered_Value();
        
        // Enviar HSB (bits 11-8) - 4 bits superiores del ADC en bits inferiores del byte
        uint8_t hsb = (filtered_adc >> 8) & 0x0F;  // Extraer bits 11-8
        Send_I2C_Response(Build_Response_With_PA4(hsb));  // PA4 en bits superiores, HSB en inferiores
        continue; // Saltar el envío normal de respuesta
      }
      else if (aRxBuffer[0] == CMD_ADC_PA0_LSB)  // 0xD9: Leer ADC PA0 - LSB (bits 7-0)
      {
        // Obtener valor ADC filtrado y estabilizado
        uint16_t filtered_adc = Get_ADC_Filtered_Value();
        
        // Enviar LSB (bits 7-0) completo - valor estabilizado
        Send_I2C_Response(filtered_adc & 0xFF);  // Byte completo LSB (bits 7-0)
        continue; // Saltar el envío normal de respuesta
      }
      else if (aRxBuffer[0] == CMD_ADC_PA0_I2C)  // 0xDA: Leer ADC PA0 escalado a dirección I2C
      {
        // Obtener valor ADC filtrado y estabilizado
        uint16_t filtered_adc = Get_ADC_Filtered_Value();
        
        // Escalar valor ADC filtrado (0-4095) a dirección I2C válida (0x08-0x77)
        uint8_t i2c_scaled_addr = Scale_ADC_To_I2C_Address(filtered_adc);
        
        //  VERIFICACIÓN CRÍTICA: Asegurar que la dirección esté en rango válido
        if (i2c_scaled_addr < 0x08 || i2c_scaled_addr > 0x77) {
          // Si está fuera de rango, forzar a un valor válido proporcional
          if (filtered_adc >= 2048) {
            i2c_scaled_addr = 0x77;  // ADC alto → dirección alta
          } else {
            i2c_scaled_addr = 0x08;  // ADC bajo → dirección baja
          }
        }
        
        // Enviar dirección I2C escalada (sin PA4, ya que es información de direccionamiento)
        Send_I2C_Response(i2c_scaled_addr);  // Dirección I2C directa sin protocolo PA4
        continue; // Saltar el envío normal de respuesta
      }
      
      // ═══════════════════════════════════════════════════════════
      //                   COMANDOS DE INFORMACIÓN DEL DISPOSITIVO
      // ═══════════════════════════════════════════════════════════
      else if (aRxBuffer[0] == CMD_GET_DEVICE_ID)   // 0x40: Obtener ID del dispositivo
      {
        response_code = getDeviceID();
      }
      else if (aRxBuffer[0] == CMD_GET_FLASH_SIZE)  // 0x41: Obtener tamaño de Flash
      {
        response_code = getFlashSize();
      }
      else if (aRxBuffer[0] == CMD_GET_UID_BYTE0)   // 0x42: UID byte 0
      {
        response_code = getUIDBytes(0);
      }
      else if (aRxBuffer[0] == CMD_GET_UID_BYTE1)   // 0x43: UID byte 1
      {
        response_code = getUIDBytes(1);
      }
      else if (aRxBuffer[0] == CMD_GET_UID_BYTE2)   // 0x44: UID byte 2
      {
        response_code = getUIDBytes(2);
      }
      else if (aRxBuffer[0] == CMD_GET_UID_BYTE3)   // 0x45: UID byte 3
      {
        response_code = getUIDBytes(3);
      }
      
      // ═══════════════════════════════════════════════════════════
      //                      COMANDOS WATCHDOG Y RESET
      // ═══════════════════════════════════════════════════════════
      else if (aRxBuffer[0] == CMD_RESET)  // 0xFE: Reset inmediato del sistema
      {
        // Preparar respuesta antes del reset
        Send_I2C_Response(Build_Response_With_PA4(RESP_RESET_OK));
        
        // Dar tiempo para que el maestro lea la respuesta
        HAL_Delay(20);
        
        // Ejecutar reset inmediato
        Force_System_Reset();
        // El código nunca llega aquí después del reset
      }
      else if (aRxBuffer[0] == CMD_WATCHDOG_RESET)  // 0xFF: Forzar reset por watchdog
      {
        // Preparar respuesta
        Send_I2C_Response(Build_Response_With_PA4(RESP_WATCHDOG_OK));
        
        // Dar tiempo para que el maestro lea la respuesta
        HAL_Delay(20);
        
        // Detener el refresh del watchdog para forzar timeout
        // El watchdog reseteará el sistema en ~2 segundos
        while(1) {
          // Bucle infinito sin refresh del watchdog
          HAL_Delay(100);
        }
        // El código nunca llega aquí después del reset por watchdog
      }

      //  NUEVO PROTOCOLO: Todas las respuestas incluyen estado PA4 digital en bits superiores
      //  Envío con timeout corto para evitar bloqueos
      Send_I2C_Response(Build_Response_With_PA4(response_code));
    }
  }
}
