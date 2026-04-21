# MapController v2.1

**Sistema Completo de Gestión y Pruebas de Dispositivos I2C para Dispositivos Compatibles PY32F003**

Librería Arduino avanzada para series ESP32/ESP32-S que proporciona gestión integral de dispositivos I2C con pruebas continuas, estadísticas y capacidades de control de dispositivos. Totalmente compatible con versiones anteriores de MapCRUD v1.x.

## ⚠️ ACTUALIZACIÓN v6.0.0_lite - Cambios Críticos de Seguridad

**IMPORTANTE**: Los comandos de relay han sido movidos al rango `0xA0-0xAF` para seguridad. Este cambio previene aperturas accidentales por corrupción I2C en buses con 20+ dispositivos.

### Comandos Relay Actualizados (CRÍTICO)
- `CMD_RELAY_OFF` = `0xA0` (antes 0x00) - Cerrar cerradura
- `CMD_RELAY_ON` = `0xA1` (antes 0x01) - Abrir cerradura  
- `CMD_TOGGLE` = `0xA6` (antes 0x06) - Pulso momentáneo (10ms)

### Nuevas Características v6.0.0_lite
- **Tonos Musicales** (0x25-0x2B): DO, RE, MI, FA, SOL, LA, SI
- **Alertas del Sistema** (0x2C-0x2F): SUCCESS, OK, WARNING, ALERT
- **PWM Gradual** (0x10-0x1F): Frecuencias 100Hz-1000Hz programables
- **Nuevos Modos de Test**: TEST_MODE_MUSICAL_SCALE, TEST_MODE_ALERTS, TEST_MODE_PWM_GRADUAL

**Razón del Cambio**: Con 160 códigos de separación entre comandos de relay (0xA0) y lectura (0x07), se previene que 1-2 bits corruptos en I2C causen aperturas accidentales.

## Características Principales

### **Gestión de Dispositivos**
- **Escaneo Inteligente de Dispositivos** - Escaneo automático del bus I2C (0x08-0x77)
- **Detección de Compatibilidad** - Verificación automática de compatibilidad de firmware PY32F003
- **Información del Dispositivo** - Lectura de ID del dispositivo, tamaño de flash y UID único
- **Gestión de Direcciones** - Cambio de direcciones I2C, reinicio de fábrica a UID

### **Sistema de Pruebas Continuas** 
- **Múltiples Modos de Prueba**: PA4, Toggle, NeoPixel, PWM, ADC, Musical Scale, Alerts, PWM Gradual
- **Salida CSV Ultra-Rápida** (v2.1) - Datos crudos: `success_rate,toggle,neo,pa4,adc_raw,vcc`
- **Estadísticas en Tiempo Real** - Tasas de éxito, seguimiento de fallos, conteo de recuperaciones
- **Intervalos Configurables** - Desde 5ms hasta 60 segundos (timing preciso)
- **Recuperación Automática** - Recuperación automática del bus I2C en caso de fallos
- **Sistema Keep-alive** - Optimizado para intervalos largos (>1s)

### **Control de Dispositivos**
- **Control de Relé** - Encendido (0xA1)/Apagado (0xA0)/Conmutación (0xA6) con comandos de seguridad
- **Gestión NeoPixel** - Rojo/Verde/Azul/Blanco/Apagado con transiciones suaves
- **Control PWM/Buzzer** - 5 niveles (Apagado, 25%, 50%, 75%, 100%) + PWM gradual (0x10-0x1F)
- **Tonos Musicales** (v6.0.0_lite) - Escala completa DO-SI (261Hz-494Hz)
- **Alertas de Sistema** (v6.0.0_lite) - SUCCESS/OK/WARNING/ALERT (800Hz-1500Hz)
- **Lectura de Sensores** - Entrada digital PA4 y lectura ADC de 12 bits PA0

### **Características Avanzadas**
- **Mapeo de Dispositivos** - Mapeo de ID a dirección I2C con persistencia
- **Interfaz de Comandos** - Sistema de comandos estilo consola enriquecido
- **Diagnósticos** - Pruebas y validación completa de dispositivos
- **Soporte Legacy** - Compatibilidad total hacia atrás con MapCRUD v1.x

## Instalación
Copie la carpeta `MapController` a su directorio de librerías de Arduino:
```
Documents/Arduino/libraries/MapController/
```

**Estructura de Archivos:**
```
MapController/
├── src/
│   ├── MapController.h     // Archivo de cabecera principal
│   └── MapController.cpp   // Implementación
├── examples/
│   └── Standalone/
│       └── Standalone.ino
├── library.properties
├── keywords.txt
└── README.md
```

## Inicio Rápido

### Uso Básico
```cpp
#include <MapController.h>

MapController gestor_dispositivos;

void setup() {
  Serial.begin(115200);
  
  // Inicializar con configuraciones optimizadas
  gestor_dispositivos.begin(Wire, 21, 22, 100000); // ESP32: SDA=21, SCL=22, 100kHz
  gestor_dispositivos.initializeI2C(1); // print=1 para salida de depuración
  
  // Escanear y mostrar dispositivos
  gestor_dispositivos.scanDevices(1);
  gestor_dispositivos.listDevices(1);
  
  // Iniciar pruebas continuas
  gestor_dispositivos.startContinuousTest(MapController::TEST_MODE_TOGGLE_ONLY);
}

void loop() {
  // Manejar pruebas continuas
  if (gestor_dispositivos.isContinuousTestActive()) {
    gestor_dispositivos.runContinuousTest();
    delay(gestor_dispositivos.getTestInterval());
  }
  
  // Mostrar estadísticas cada 30 segundos
  static unsigned long ultimasEstadisticas = 0;
  if (millis() - ultimasEstadisticas > 30000) {
    gestor_dispositivos.showStatistics(1);
    ultimasEstadisticas = millis();
  }
}
```



## Ejemplos de Uso

### Control de Dispositivos
```cpp
// Control directo por dirección
gestor_dispositivos.cmdRelay(0x20, true, 1);        // Activar relé en dirección 0x20
gestor_dispositivos.cmdNeoRed(0x20, 1);              // Configurar NeoPixels a rojo
gestor_dispositivos.cmdPWM75(0x20, 1);               // Configurar PWM al 75% (1000Hz)
gestor_dispositivos.cmdToggle(0x20, 1);              // Conmutar relé (pulso)

// Leer sensores
uint8_t estado_pa4 = gestor_dispositivos.readPA4Digital(0x20);
uint16_t valor_adc = gestor_dispositivos.readADC(0x20);
```

### Pruebas Continuas
```cpp
// Iniciar diferentes modos de prueba
gestor_dispositivos.startContinuousTest(MapController::TEST_MODE_PA4_ONLY);     // Solo lectura PA4
gestor_dispositivos.startContinuousTest(MapController::TEST_MODE_TOGGLE_ONLY);  // Solo conmutación de relé
gestor_dispositivos.startContinuousTest(MapController::TEST_MODE_NEO_CYCLE);    // Ciclo de colores NeoPixel
gestor_dispositivos.startContinuousTest(MapController::TEST_MODE_PWM_TONES);    // Ciclo de tonos PWM
gestor_dispositivos.startContinuousTest(MapController::TEST_MODE_BOTH);         // Conmutación + PA4

// Configurar intervalo de prueba
gestor_dispositivos.setTestInterval(1000); // 1 segundo entre pruebas

// Obtener estadísticas
gestor_dispositivos.showStatistics(1);
gestor_dispositivos.resetStatistics();
```

### Gestión de Direcciones I2C
```cpp
// Cambiar dirección del dispositivo de 0x20 a 0x30
gestor_dispositivos.changeI2CAddress(0x20, 0x30, 1);

// Reinicio de fábrica del dispositivo para usar dirección basada en UID
gestor_dispositivos.factoryReset(0x20, 1);

// Verificar estado I2C (dirección Flash vs UID)
gestor_dispositivos.checkI2CStatus(0x20, 1);

// Reiniciar dispositivo
gestor_dispositivos.resetDevice(0x20, 1);
```

### Información del Dispositivo
```cpp
// Mostrar información completa del dispositivo
gestor_dispositivos.showDeviceInfo(0x20, 1);

// Obtener UID del dispositivo
uint32_t uid = gestor_dispositivos.getDeviceUID(0x20);

// Mostrar detalles del UID del dispositivo
gestor_dispositivos.showDeviceUID(0x20, 1);

// Probar todos los comandos del dispositivo
gestor_dispositivos.testAllCommands(0x20, 1);
```

## Constantes de Comandos

### Modos de Prueba
```cpp
MapController::TEST_MODE_PA4_ONLY      // Solo lectura digital PA4
MapController::TEST_MODE_TOGGLE_ONLY   // Solo conmutación de relé
MapController::TEST_MODE_BOTH          // Conmutación + lectura PA4
MapController::TEST_MODE_NEO_CYCLE     // Ciclo de colores NeoPixel (W→R→G→B→OFF)
MapController::TEST_MODE_PWM_TONES     // Ciclo de tonos PWM (OFF→25→50→75→100)
MapController::TEST_MODE_ADC_PA0       // Solo lectura ADC PA0
```

### Comandos de Dispositivos
```cpp
// Comandos básicos de relé
CMD_RELAY_OFF, CMD_RELAY_ON, CMD_TOGGLE

// Comandos NeoPixel
CMD_NEO_RED, CMD_NEO_GREEN, CMD_NEO_BLUE, CMD_NEO_WHITE, CMD_NEO_OFF

// Comandos PWM/Buzzer
CMD_PWM_OFF, CMD_PWM_25, CMD_PWM_50, CMD_PWM_75, CMD_PWM_100

// Comandos de sensores
CMD_PA4_DIGITAL, CMD_ADC_PA0_HSB, CMD_ADC_PA0_LSB

// Comandos de información del dispositivo
CMD_GET_DEVICE_ID, CMD_GET_FLASH_SIZE, CMD_GET_UID_BYTE0-3

// Gestión I2C
CMD_SET_I2C_ADDR, CMD_RESET_FACTORY, CMD_GET_I2C_STATUS, CMD_RESET
```

## Configuración

### Configuraciones I2C por Defecto
```cpp
DEFAULT_SDA = 21           // Pin SDA ESP32
DEFAULT_SCL = 22           // Pin SCL ESP32
DEFAULT_I2C_FREQ = 100000  // 100kHz para estabilidad
```

### Configuraciones del Sistema de Pruebas
```cpp
DEFAULT_TEST_INTERVAL = 2000  // 2 segundos por defecto
MIN_TEST_INTERVAL = 5         // Mínimo 5ms
MAX_TEST_INTERVAL = 60000     // Máximo 60 segundos
```

## Estadísticas y Monitoreo

El sistema proporciona estadísticas detalladas para cada dispositivo:
- **Pruebas Totales** - Número de intentos de comunicación
- **Tasa de Éxito** - Porcentaje de comunicaciones exitosas
- **Fallos Consecutivos** - Racha actual de fallos
- **Tiempo de Último Éxito/Fallo** - Marcas de tiempo para depuración
- **Recuperaciones del Bus** - Conteo de reinicios del bus I2C
- **Valores de Sensores** - Últimas lecturas PA4 y ADC

## Guía de Migración

### De MapCRUD v1.x a MapController v2.0

**Migración de Librería:**
```cpp
// Cambiar el include y nombre de librería
// ANTERIOR: #include <MapCRUD.h>
#include <MapController.h>

// Cambiar nombre de clase (alias MapCRUD aún disponible para compatibilidad)
MapController gestor_dispositivos;  // Recomendado
// O
MapCRUD gestor_dispositivos;       // Aún funciona (alias)
```

**Compatibilidad de Comandos:**
Todos los comandos y funciones existentes de MapCRUD v1.x son totalmente compatibles:
- `cmdShowMapping()`, `cmdAutoMap()`, `scanDevices()`
- `assignDeviceToID()`, `getAddressByID()`, etc.
- Todas las funciones de relé, NeoPixel y sensores funcionan sin cambios

**Nuevas Características Disponibles:**
- Sistema de pruebas continuas con múltiples modos
- Control PWM/Buzzer (CMD_PWM_25, CMD_PWM_50, etc.)
- Gestión avanzada de direcciones I2C
- Información del dispositivo y lectura de UID
- Estadísticas y monitoreo en tiempo real

## Compatibilidad de Hardware

### Microcontroladores Soportados
- ESP32 (todas las variantes)
- ESP32-S2, ESP32-S3
- ESP32-C3, ESP32-C6

### Dispositivos Compatibles
- **PY32F003F16U6TR** (16KB Flash)
- **PY32F003F18U6TR** (18KB Flash)
- Cualquier dispositivo I2C que implemente el protocolo de comandos

### Configuración I2C
- **Frecuencia**: 100kHz recomendado para estabilidad
- **Rango de Direcciones**: 0x08 a 0x77 (rango I2C estándar)
- **Pull-ups**: 4.7kΩ externos recomendados para confiabilidad

## Referencia Rápida de Comandos I2C v6.0.0_lite

### Comandos Críticos de Seguridad (0xA0-0xAF)
```cpp
// ⚠️ COMANDOS DE RELAY - Rango de seguridad separado
gestor.sendBasicCommand(addr, MapController::CMD_RELAY_OFF);  // 0xA0 - Cerrar
gestor.sendBasicCommand(addr, MapController::CMD_RELAY_ON);   // 0xA1 - Abrir
gestor.sendBasicCommand(addr, MapController::CMD_TOGGLE);     // 0xA6 - Pulso 10ms
```

### Comandos de Efectos Visuales (0x02-0x08)
```cpp
gestor.cmdNeoRed(addr, 1);     // 0x02 - Rojo
gestor.cmdNeoGreen(addr, 1);   // 0x03 - Verde
gestor.cmdNeoBlue(addr, 1);    // 0x04 - Azul
gestor.cmdNeoOff(addr, 1);     // 0x05 - Apagar
gestor.cmdNeoWhite(addr, 1);   // 0x08 - Blanco
```

### Comandos PWM/Audio (0x10-0x2F)
```cpp
// PWM Gradual (0x10-0x1F): 100Hz-1000Hz
gestor.cmdPWMGradual(addr, 0x10, 1);  // 100Hz
gestor.cmdPWMGradual(addr, 0x18, 1);  // ~550Hz
gestor.cmdPWMGradual(addr, 0x1F, 1);  // 1000Hz

// Tonos predefinidos
gestor.cmdPWMOff(addr, 1);    // 0x20 - OFF
gestor.cmdPWM25(addr, 1);     // 0x21 - 200Hz
gestor.cmdPWM50(addr, 1);     // 0x22 - 500Hz
gestor.cmdPWM75(addr, 1);     // 0x23 - 1000Hz
gestor.cmdPWM100(addr, 1);    // 0x24 - 2000Hz

// Notas musicales (0x25-0x2B)
gestor.cmdToneDo(addr, 1);    // 0x25 - 261Hz
gestor.cmdToneRe(addr, 1);    // 0x26 - 294Hz
gestor.cmdToneMi(addr, 1);    // 0x27 - 330Hz
gestor.cmdToneFa(addr, 1);    // 0x28 - 349Hz
gestor.cmdToneSol(addr, 1);   // 0x29 - 392Hz
gestor.cmdToneLa(addr, 1);    // 0x2A - 440Hz
gestor.cmdToneSi(addr, 1);    // 0x2B - 494Hz

// Alertas del sistema (0x2C-0x2F)
gestor.cmdSuccess(addr, 1);   // 0x2C - 800Hz
gestor.cmdOk(addr, 1);        // 0x2D - 1000Hz
gestor.cmdWarning(addr, 1);   // 0x2E - 1200Hz
gestor.cmdAlert(addr, 1);     // 0x2F - 1500Hz
```

### Comandos de Lectura (0x00-0x0F)
```cpp
uint8_t pa4 = gestor.readPA4Digital(addr);     // 0x07 - Lectura digital PA4
uint16_t adc = gestor.readADC(addr);           // 0xD8/D9 - ADC 12-bit (0-4095)
```

### Modos de Test Continuo
```cpp
// Modos disponibles
gestor.startContinuousTest(MapController::TEST_MODE_PA4_ONLY);        // Solo lectura PA4
gestor.startContinuousTest(MapController::TEST_MODE_TOGGLE_ONLY);     // Solo toggle relay
gestor.startContinuousTest(MapController::TEST_MODE_NEO_CYCLE);       // Ciclo NeoPixels
gestor.startContinuousTest(MapController::TEST_MODE_PWM_TONES);       // Ciclo PWM estándar
gestor.startContinuousTest(MapController::TEST_MODE_MUSICAL_SCALE);   // 🎵 Escala DO-SI
gestor.startContinuousTest(MapController::TEST_MODE_ALERTS);          // 🔔 Ciclo alertas
gestor.startContinuousTest(MapController::TEST_MODE_PWM_GRADUAL);     // 📈 PWM gradual
gestor.startContinuousTest(MapController::TEST_MODE_ADC_PA0);         // Solo ADC
```

## Solución de Problemas

### Problemas Comunes

**No se encuentran dispositivos durante el escaneo**
- Verificar conexiones I2C (SDA/SCL)
- Verificar fuente de alimentación del dispositivo
- Asegurar que las resistencias pull-up estén presentes
- Intentar frecuencia I2C más baja (50kHz)

**Alta tasa de fallos en pruebas continuas**
- Reducir intervalo de prueba (`setTestInterval(500)`)
- Verificar integridad de señal I2C
- Verificar compatibilidad del dispositivo
- Habilitar recuperación automática

**Dispositivo no responde después de cambio de dirección**
- Esperar 3-5 segundos después del cambio de dirección
- Re-escanear el bus para encontrar nueva dirección
- Usar reinicio de fábrica si el dispositivo se pierde
- Verificar requisito de ciclo de energía

## Licencia

Esta librería se proporciona bajo Licencia MIT. Vea el archivo LICENSE para detalles.

## Contribuciones

¡Las contribuciones son bienvenidas! Por favor siéntase libre de enviar un Pull Request.

## Soporte

Para soporte y preguntas:
- Email: support@unit-electronics.com
- GitHub Issues: [Crear un issue](https://github.com/unit-electronics/MapController/issues)
