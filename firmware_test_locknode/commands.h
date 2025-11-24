#ifndef __COMMANDS_H
#define __COMMANDS_H

#include "main.h"

#define DATA_LENGTH       1
#define I2C_SPEEDCLOCK    400000
#define I2C_DUTYCYCLE     I2C_DUTYCYCLE_2
#define I2C_TIMEOUT       100  // Aumentado de 300 a 1000ms
#define MAX_I2C_ERRORS    10

#define I2C_ADDRESS_COUNT 128

// Direcciones UID para PY32F0xx - Múltiples posibles ubicaciones
#define UID_BASE_ADDRESS   0x1FFF0E00UL   // Dirección base del UID principal
#define UID_ALT_ADDRESS    0x1FFF0C00UL   // Dirección alternativa
#define UID_SIZE           12             // 12 bytes (96 bits)

// Rango de direcciones I2C dinámicas (7-bit addressing)
#define I2C_ADDR_MIN       0x08         // Dirección mínima (evitar direcciones reservadas)
#define I2C_ADDR_MAX       0x77         // Dirección máxima (evitar direcciones reservadas)

// ═══════════════════════════════════════════════════════════
//           COMANDOS CRÍTICOS DE SEGURIDAD (0xA0-0xAF)
// ═══════════════════════════════════════════════════════════
// RANGO SEPARADO: Comandos que controlan RELAY usan rango 0xA0-0xAF
// para evitar confusión con comandos de lectura/status
#define CMD_RELAY_OFF      0xA0         // Comando: Apagar RELÉ PB5 (SEGURIDAD)
#define CMD_RELAY_ON       0xA1         // Comando: Encender RELÉ PB5 (SEGURIDAD)
#define CMD_TOGGLE         0xA6         // Comando: Alternar estado de RELÉ PB5 (SEGURIDAD)

// ═══════════════════════════════════════════════════════════
//           COMANDOS DE LECTURA/STATUS (0x00-0x0F)
// ═══════════════════════════════════════════════════════════
#define CMD_PA4_DIGITAL    0x07         // Comando: Leer PA4 como entrada digital

// ═══════════════════════════════════════════════════════════
//           COMANDOS DE EFECTOS VISUALES (0x02-0x08)
// ═══════════════════════════════════════════════════════════
#define CMD_RED            0x02         // Comando: NeoPixels rojos
#define CMD_GREEN          0x03         // Comando: NeoPixels verdes
#define CMD_BLUE           0x04         // Comando: NeoPixels azules
#define CMD_OFF            0x05         // Comando: NeoPixels apagados
#define CMD_WHITE          0x08         // Comando: NeoPixels blancos

// Comandos PWM con frecuencias específicas
#define CMD_PWM_MIN        0x10         // PWM mínimo (rango 0x10-0x1F)
#define CMD_PWM_MAX        0x1F         // PWM máximo (rango 0x10-0x1F)
#define CMD_PWM_OFF        0x20         // PWM OFF - Silencio
#define CMD_PWM_25         0x21         // PWM 25% - Tono grave 200Hz
#define CMD_PWM_50         0x22         // PWM 50% - Tono medio 500Hz
#define CMD_PWM_75         0x23         // PWM 75% - Tono agudo 1000Hz
#define CMD_PWM_100        0x24         // PWM 100% - Tono muy agudo 2000Hz

// Nuevos comandos para tonos específicos
#define CMD_TONE_DO        0x25         // Tono DO (261Hz)
#define CMD_TONE_RE        0x26         // Tono RE (294Hz)
#define CMD_TONE_MI        0x27         // Tono MI (330Hz)
#define CMD_TONE_FA        0x28         // Tono FA (349Hz)
#define CMD_TONE_SOL       0x29         // Tono SOL (392Hz)
#define CMD_TONE_LA        0x2A         // Tono LA (440Hz)
#define CMD_TONE_SI        0x2B         // Tono SI (494Hz)

// Comandos para alertas del sistema
#define CMD_SUCCESS        0x2C         // Success - Tono positivo 800Hz
#define CMD_OK             0x2D         // OK - Confirmación 1000Hz
#define CMD_WARNING        0x2E         // Warning - Advertencia 1200Hz
#define CMD_ALERT          0x2F         // Alert - Alerta crítica 1500Hz

// ═══════════════════════════════════════════════════════════
//                      COMANDOS DE RESET Y WATCHDOG
// ═══════════════════════════════════════════════════════════
#define CMD_RESET          0xFE         // Comando: Reset inmediato del sistema
#define CMD_WATCHDOG_RESET 0xFF         // Comando: Forzar reset por watchdog

// Configuración del watchdog independiente (IWDG)
#define WATCHDOG_TIMEOUT   2000         // Timeout del watchdog: 2 segundos
#define WATCHDOG_REFRESH   1500         // Refrescar watchdog cada 1.5 segundos

// ═══════════════════════════════════════════════════════════
//                      COMANDOS ADC PA0 (12-bit)
// ═══════════════════════════════════════════════════════════
#define CMD_ADC_PA0_HSB    0xD8         // Leer ADC PA0 - HSB (bits 11-8)
#define CMD_ADC_PA0_LSB    0xD9         // Leer ADC PA0 - LSB (bits 7-0)
#define CMD_ADC_PA0_I2C    0xDA         // Leer ADC PA0 escalado a dirección I2C (7-bit)

// ═══════════════════════════════════════════════════════════
//                      COMANDOS FLASH DATA (Test)
// ═══════════════════════════════════════════════════════════
#define SAVE_DATA          0x3A         // Comando: Guardar datos en Flash
#define READ_DATA          0x3B         // Comando: Leer datos desde Flash

// ═══════════════════════════════════════════════════════════
//                   COMANDOS I2C ADDRESS MANAGEMENT (v5_4)
// ═══════════════════════════════════════════════════════════
#define CMD_SET_I2C_ADDR   0x3D         // Comando: Establecer nueva dirección I2C (se guarda en Flash)
#define CMD_RESET_FACTORY  0x3E         // Comando: Reset factory (usar UID por defecto)
#define CMD_GET_I2C_STATUS 0x3F         // Comando: Obtener estado I2C (Flash vs UID)

// Respuesta para comandos Flash
#define RESPONSE_DATA      0x3C         // Respuesta: Operación Flash completada

// ═══════════════════════════════════════════════════════════
//                   RESPUESTAS I2C ADDRESS MANAGEMENT
// ═══════════════════════════════════════════════════════════
#define RESP_I2C_ADDR_SET  0x0D         // Nueva dirección I2C establecida
#define RESP_FACTORY_RESET 0x0E         // Reset factory ejecutado (usar UID)
#define RESP_I2C_FROM_FLASH 0x0F        // I2C usando dirección desde Flash
#define RESP_I2C_FROM_UID  0x0A         // I2C usando dirección desde UID

// Códigos de respuesta para indicar el estado de la operación
//  NUEVO PROTOCOLO: Solo bits inferiores (3-0), PA4 DIGITAL en bits superiores (7-4)
#define RESP_RELAY_OFF     0x00         // RELÉ PB5 apagado exitosamente
#define RESP_RELAY_ON      0x01         // RELÉ PB5 encendido exitosamente
#define RESP_NEO_RED       0x02         // NeoPixels rojos activados
#define RESP_NEO_GREEN     0x03         // NeoPixels verdes activados
#define RESP_NEO_BLUE      0x04         // NeoPixels azules activados
#define RESP_NEO_OFF       0x05         // NeoPixels apagados
#define RESP_TOGGLE        0x06         // RELÉ PB5 alternado exitosamente
#define RESP_NEO_WHITE     0x08         // NeoPixels blancos activados
#define RESP_PWM_SET       0x06         // PWM configurado exitosamente
#define RESP_PWM_OFF       0x07         // PWM desactivado
#define RESP_PA4_DIGITAL   0x09         // Estado digital de PA4 enviado
#define RESP_TONE_SET      0x0C         // Tono configurado exitosamente
#define RESP_SUCCESS       0x0D         // Success ejecutado
#define RESP_OK            0x0E         // OK ejecutado
#define RESP_WARNING       0x0F         // Warning ejecutado
#define RESP_ALERT         0x0A         // Alert ejecutado
#define RESP_CMD_UNKNOWN   0x0B         // Comando no reconocido
#define RESP_I2C_ERROR     0x08         // Error de comunicación

//  CÓDIGOS DE RESPUESTA ADC PA0
#define RESP_ADC_HSB       0x0D         // ADC HSB enviado (bits 11-8)
#define RESP_ADC_LSB       0x0E         // ADC LSB enviado (bits 7-0)
#define RESP_ADC_I2C       0x0F         // ADC escalado a dirección I2C enviado

//  CÓDIGOS DE RESPUESTA WATCHDOG Y RESET
#define RESP_RESET_OK      0x0F         // Reset ejecutado exitosamente
#define RESP_WATCHDOG_OK   0x0A         // Watchdog reset forzado

#endif
