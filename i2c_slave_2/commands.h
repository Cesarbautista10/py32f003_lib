#ifndef __COMMANDS_H
#define __COMMANDS_H

#include "main.h"

#define DATA_LENGTH       1
#define I2C_SPEEDCLOCK    400000
#define I2C_DUTYCYCLE     I2C_DUTYCYCLE_2
#define I2C_TIMEOUT       20  // 20ms - timeout corto para respuesta rápida
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
#define CMD_RELAY_OFF      0xA0         // Comando: Apagar RELÉ PA2 y PB5 (SEGURIDAD)
#define CMD_RELAY_ON       0xA1         // Comando: Encender RELÉ PA2 y PB5 (SEGURIDAD)
#define CMD_RELAY_TOGGLE   0xA6         // Comando: Pulso/Disparo RELÉ PA2 y PB5 (SEGURIDAD)

// ═══════════════════════════════════════════════════════════
//           COMANDOS DE EFECTOS VISUALES (0x02-0x08)
// ═══════════════════════════════════════════════════════════
#define CMD_RED            0x02         // Comando: NeoPixels rojos
#define CMD_GREEN          0x03         // Comando: NeoPixels verdes
#define CMD_BLUE           0x04         // Comando: NeoPixels azules
#define CMD_OFF            0x05         // Comando: NeoPixels apagados

// ═══════════════════════════════════════════════════════════
//                      COMANDOS ENTRADAS DIGITALES
// ═══════════════════════════════════════════════════════════
#define CMD_PA4_DIGITAL    0x07         // Comando: Leer PA4 como entrada digital
#define CMD_PB0_DIGITAL    0x06         // Comando: Leer PB0 como entrada digital (requiere PF2-NRST deshabilitado)

// ═══════════════════════════════════════════════════════════
//                    COMANDOS PF2-NRST CONFIGURATION
// ═══════════════════════════════════════════════════════════
#define CMD_DISABLE_NRST   0x30         // Comando: Deshabilitar PF2-NRST (configurar como GPIO)
#define CMD_CHECK_NRST     0x31         // Comando: Verificar estado PF2-NRST
#define CMD_INIT_PB0       0x32         // Comando: Inicializar PB0 como entrada digital

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

// ═══════════════════════════════════════════════════════════
//                   RESPUESTAS PF2-NRST CONFIGURATION
// ═══════════════════════════════════════════════════════════
#define RESP_NRST_DISABLED 0x10         // PF2-NRST deshabilitado exitosamente
#define RESP_NRST_GPIO     0x11         // PF2 configurado como GPIO
#define RESP_NRST_ACTIVE   0x12         // PF2 sigue como NRST (reset activo)
#define RESP_PB0_DIGITAL   0x13         // Estado digital de PB0 enviado
#define RESP_PB0_INITIALIZED 0x14       // PB0 inicializado como entrada digital

// Códigos de respuesta para indicar el estado de la operación
//  NUEVO PROTOCOLO: Solo bits inferiores (3-0), PA4 DIGITAL en bits superiores (7-4)
#define RESP_RELAY_OFF     0x00         // RELÉ PA2 y PB5 apagado exitosamente
#define RESP_RELAY_ON      0x01         // RELÉ PA2 y PB5 encendido exitosamente
#define RESP_NEO_RED       0x02         // NeoPixels rojos activados
#define RESP_NEO_GREEN     0x03         // NeoPixels verdes activados
#define RESP_NEO_BLUE      0x04         // NeoPixels azules activados
#define RESP_NEO_OFF       0x05         // NeoPixels apagados
#define RESP_RELAY_TOGGLE  0x06         // RELÉ PA2 y PB5 pulso ejecutado
#define RESP_PA4_DIGITAL   0x09         // Estado digital de PA4 enviado
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
