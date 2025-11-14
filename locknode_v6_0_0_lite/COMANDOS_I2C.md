# Comandos I2C - PY32F003 Locknode v5.4.2 Lite

## Índice
- [Comandos Críticos de Seguridad](#comandos-críticos-de-seguridad-0xa0-0xaf)
- [Comandos de Lectura/Status](#comandos-de-lecturastatus-0x00-0x0f)
- [Comandos de Efectos Visuales](#comandos-de-efectos-visuales-0x02-0x08)
- [Comandos PWM/Audio](#comandos-pwmaudio-0x10-0x2f)
- [Comandos ADC](#comandos-adc-0xd8-0xda)
- [Comandos Flash](#comandos-flash-0x3a-0x3b)
- [Comandos Gestión I2C](#comandos-gestión-i2c-0x3d-0x3f)
- [Comandos Información Dispositivo](#comandos-información-dispositivo-0x40-0x45)
- [Comandos Reset](#comandos-reset-0xfe-0xff)

---

## Comandos Críticos de Seguridad (0xA0-0xAF)

**⚠️ IMPORTANTE**: Estos comandos controlan el relay de apertura y están en un rango separado (0xA0-0xAF) para evitar confusión con comandos de lectura/status. Esto previene aperturas accidentales por errores de transmisión I2C.

### 0xA0 - CMD_RELAY_OFF
**Descripción**: Apagar relay en pin PB5  
**Función**: Bloquear/cerrar cerradura  
**Respuesta**: `0xB0` (RESP_RELAY_OFF)  
**Seguridad**: Comando seguro, cierra el relay  
**Ejemplo**: `Wire.write(0xA0);`

### 0xA1 - CMD_RELAY_ON
**Descripción**: Encender relay en pin PB5  
**Función**: Desbloquear/abrir cerradura  
**Respuesta**: `0xB1` (RESP_RELAY_ON)  
**Seguridad**: ⚠️ CRÍTICO - Abre la cerradura  
**Ejemplo**: `Wire.write(0xA1);`

### 0xA6 - CMD_TOGGLE
**Descripción**: Alternar estado del relay (pulso de 10ms)  
**Función**: Pulso corto para activación momentánea  
**Respuesta**: `0xB6` (RESP_TOGGLE)  
**Timing**: Relay ON durante 10ms, luego OFF automáticamente  
**Seguridad**: ⚠️ CRÍTICO - Abre temporalmente  
**Ejemplo**: `Wire.write(0xA6);`

---

## Comandos de Lectura/Status (0x00-0x0F)

### 0x07 - CMD_PA4_DIGITAL
**Descripción**: Leer estado digital del pin PA4  
**Función**: Obtener estado de sensor/entrada digital  
**Respuesta**: Estado PA4 en bits superiores [7-4] + código `0x09` en bits inferiores [3-0]  
**Valores**:
- Bits [7-4]: `0000` = PA4 LOW (0V), `0001` = PA4 HIGH (3.3V)
- Bits [3-0]: `0x09` (código respuesta)
**Ejemplo**:
```cpp
Wire.write(0x07);
uint8_t resp = Wire.read();
bool pa4_state = (resp >> 4) & 0x01; // Extraer bit superior
```

---

## Comandos de Efectos Visuales (0x02-0x08)

**Nota**: Estos comandos controlan NeoPixels WS2812B conectados al dispositivo.

### 0x02 - CMD_RED
**Descripción**: Activar NeoPixels en color rojo  
**Respuesta**: `0x02` (RESP_NEO_RED)  
**Ejemplo**: `Wire.write(0x02);`

### 0x03 - CMD_GREEN
**Descripción**: Activar NeoPixels en color verde  
**Respuesta**: `0x03` (RESP_NEO_GREEN)  
**Ejemplo**: `Wire.write(0x03);`

### 0x04 - CMD_BLUE
**Descripción**: Activar NeoPixels en color azul  
**Respuesta**: `0x04` (RESP_NEO_BLUE)  
**Ejemplo**: `Wire.write(0x04);`

### 0x05 - CMD_OFF
**Descripción**: Apagar todos los NeoPixels  
**Respuesta**: `0x05` (RESP_NEO_OFF)  
**Ejemplo**: `Wire.write(0x05);`

### 0x08 - CMD_WHITE
**Descripción**: Activar NeoPixels en color blanco  
**Respuesta**: `0x08` (RESP_NEO_WHITE)  
**Ejemplo**: `Wire.write(0x08);`

---

## Comandos PWM/Audio (0x10-0x2F)

**Nota**: Estos comandos controlan el buzzer PWM en pin PB6 (TIM1_CH3).

### Rango PWM Gradual (0x10-0x1F)

**Descripción**: Generar tono con frecuencia variable  
**Rango**: 0x10 (100Hz) hasta 0x1F (1000Hz)  
**Fórmula**: `frecuencia = 100 + ((comando - 0x10) * 900 / 16)`  
**Respuesta**: `0x06` (RESP_PWM_SET)  
**Ejemplo**:
```cpp
Wire.write(0x10); // 100Hz (tono muy grave)
Wire.write(0x18); // ~550Hz (tono medio)
Wire.write(0x1F); // 1000Hz (tono agudo)
```

### 0x20 - CMD_PWM_OFF
**Descripción**: Apagar PWM/buzzer completamente  
**Respuesta**: `0x07` (RESP_PWM_OFF)  
**Ejemplo**: `Wire.write(0x20);`

### Tonos Predefinidos

| Comando | Hex  | Frecuencia | Descripción |
|---------|------|------------|-------------|
| CMD_PWM_25 | 0x21 | 200Hz | Tono grave |
| CMD_PWM_50 | 0x22 | 500Hz | Tono medio |
| CMD_PWM_75 | 0x23 | 1000Hz | Tono agudo |
| CMD_PWM_100 | 0x24 | 2000Hz | Tono muy agudo |

**Respuesta**: `0x06` (RESP_PWM_SET)

### Notas Musicales (0x25-0x2B)

| Comando | Hex  | Nota | Frecuencia |
|---------|------|------|------------|
| CMD_TONE_DO | 0x25 | DO | 261Hz |
| CMD_TONE_RE | 0x26 | RE | 294Hz |
| CMD_TONE_MI | 0x27 | MI | 330Hz |
| CMD_TONE_FA | 0x28 | FA | 349Hz |
| CMD_TONE_SOL | 0x29 | SOL | 392Hz |
| CMD_TONE_LA | 0x2A | LA | 440Hz |
| CMD_TONE_SI | 0x2B | SI | 494Hz |

**Respuesta**: `0x0C` (RESP_TONE_SET)  
**Ejemplo**:
```cpp
// Tocar melodía DO-RE-MI
Wire.write(0x25); delay(200);
Wire.write(0x26); delay(200);
Wire.write(0x27); delay(200);
Wire.write(0x20); // Silencio
```

### Alertas del Sistema (0x2C-0x2F)

| Comando | Hex  | Tipo | Frecuencia | Uso |
|---------|------|------|------------|-----|
| CMD_SUCCESS | 0x2C | Éxito | 800Hz | Operación exitosa |
| CMD_OK | 0x2D | Confirmación | 1000Hz | Comando aceptado |
| CMD_WARNING | 0x2E | Advertencia | 1200Hz | Atención requerida |
| CMD_ALERT | 0x2F | Alerta | 1500Hz | Error crítico |

**Respuestas**:
- CMD_SUCCESS: `0x0D` (RESP_SUCCESS)
- CMD_OK: `0x0E` (RESP_OK)
- CMD_WARNING: `0x0F` (RESP_WARNING)
- CMD_ALERT: `0x0A` (RESP_ALERT)

---

## Comandos ADC (0xD8-0xDA)

**Nota**: Lectura de ADC de 12 bits en pin PA0.

### 0xD8 - CMD_ADC_PA0_HSB
**Descripción**: Leer bits superiores [11-8] del ADC PA0  
**Respuesta**: Valor HSB (High Significant Byte)  
**Rango**: 0x00-0x0F (4 bits superiores)  
**Ejemplo**:
```cpp
Wire.write(0xD8);
uint8_t hsb = Wire.read(); // Bits 11-8
```

### 0xD9 - CMD_ADC_PA0_LSB
**Descripción**: Leer bits inferiores [7-0] del ADC PA0  
**Respuesta**: Valor LSB (Low Significant Byte)  
**Rango**: 0x00-0xFF (8 bits inferiores)  
**Ejemplo**:
```cpp
Wire.write(0xD9);
uint8_t lsb = Wire.read(); // Bits 7-0
```

**Reconstruir valor completo de 12 bits**:
```cpp
Wire.write(0xD8);
uint8_t hsb = Wire.read();
Wire.write(0xD9);
uint8_t lsb = Wire.read();
uint16_t adc_value = ((hsb & 0x0F) << 8) | lsb; // 12-bit (0-4095)
```

### 0xDA - CMD_ADC_PA0_I2C
**Descripción**: Leer ADC PA0 escalado a dirección I2C (7-bit)  
**Respuesta**: Valor ADC escalado al rango 0x08-0x77  
**Uso**: Mapear valor analógico a dirección I2C dinámica  
**Ejemplo**:
```cpp
Wire.write(0xDA);
uint8_t i2c_addr = Wire.read(); // Rango 0x08-0x77
```

---

## Comandos Flash (0x3A-0x3B)

### 0x3A - SAVE_DATA
**Descripción**: Activar modo guardado en Flash  
**Funcionamiento**: El siguiente byte recibido se guardará en Flash  
**Dirección Flash**: `0x08003C00`  
**Respuesta**: `0x3C` (RESPONSE_DATA)  
**Ejemplo**:
```cpp
Wire.write(0x3A);        // Activar modo guardado
Wire.endTransmission();
Wire.beginTransmission(addr);
Wire.write(0x55);        // Guardar valor 0x55
Wire.endTransmission();
```

### 0x3B - READ_DATA
**Descripción**: Leer dato guardado en Flash  
**Dirección Flash**: `0x08003C00`  
**Respuesta**: Byte leído desde Flash  
**Ejemplo**:
```cpp
Wire.write(0x3B);
uint8_t data = Wire.read(); // Leer dato guardado
```

---

## Comandos Gestión I2C (0x3D-0x3F)

### 0x3D - CMD_SET_I2C_ADDR
**Descripción**: Establecer nueva dirección I2C personalizada  
**Funcionamiento**: El siguiente byte recibido será la nueva dirección  
**Rango válido**: 0x08-0x77  
**Persistencia**: Se guarda en Flash (`0x08003E00`)  
**Respuesta**: `0x0D` (RESP_I2C_ADDR_SET)  
**Ejemplo**:
```cpp
Wire.write(0x3D);        // Activar modo cambio dirección
Wire.endTransmission();
Wire.beginTransmission(addr);
Wire.write(0x50);        // Nueva dirección: 0x50
Wire.endTransmission();
// Reiniciar dispositivo para aplicar cambio
```

### 0x3E - CMD_RESET_FACTORY
**Descripción**: Reset a configuración de fábrica  
**Efecto**: Volver a usar dirección I2C calculada desde UID  
**Respuesta**: `0x0E` (RESP_FACTORY_RESET)  
**Ejemplo**: `Wire.write(0x3E);`

### 0x3F - CMD_GET_I2C_STATUS
**Descripción**: Obtener estado de configuración I2C  
**Respuesta**:
- `0x0F` (RESP_I2C_FROM_FLASH): Usando dirección desde Flash
- `0x0A` (RESP_I2C_FROM_UID): Usando dirección desde UID
**Ejemplo**:
```cpp
Wire.write(0x3F);
uint8_t status = Wire.read();
if (status == 0x0F) {
  Serial.println("I2C desde Flash (personalizada)");
} else {
  Serial.println("I2C desde UID (fábrica)");
}
```

---

## Comandos Información Dispositivo (0x40-0x45)

### 0x40 - CMD_GET_DEVICE_ID
**Descripción**: Obtener ID del dispositivo (F16/F18)  
**Respuesta**:
- `0xF1`: PY32F003F16 (16KB Flash)
- `0xF8`: PY32F003F18 (18KB Flash)
**Ejemplo**:
```cpp
Wire.write(0x40);
uint8_t device_id = Wire.read();
```

### 0x41 - CMD_GET_FLASH_SIZE
**Descripción**: Obtener tamaño de Flash en KB  
**Respuesta**: Tamaño en KB (16 o 18)  
**Ejemplo**:
```cpp
Wire.write(0x41);
uint8_t flash_kb = Wire.read(); // 16 o 18
```

### 0x42-0x45 - CMD_GET_UID_BYTEx
**Descripción**: Obtener bytes del UID único del dispositivo  
**UID**: 96 bits (12 bytes) únicos por chip  
**Comandos**:
- `0x42`: UID byte 0 (LSB)
- `0x43`: UID byte 1
- `0x44`: UID byte 2
- `0x45`: UID byte 3 (MSB)

**Ejemplo**:
```cpp
uint8_t uid[4];
for (int i = 0; i < 4; i++) {
  Wire.write(0x42 + i);
  uid[i] = Wire.read();
}
Serial.printf("UID: %02X%02X%02X%02X\n", uid[3], uid[2], uid[1], uid[0]);
```

---

## Comandos Reset (0xFE-0xFF)

### 0xFE - CMD_RESET
**Descripción**: Reset inmediato del sistema  
**Método**: Software reset usando NVIC  
**Respuesta**: `0x0F` (RESP_RESET_OK) antes de reiniciar  
**Ejemplo**: `Wire.write(0xFE);`

### 0xFF - CMD_WATCHDOG_RESET
**Descripción**: Forzar reset por watchdog  
**Método**: Deshabilitar refresh del IWDG  
**Timeout**: ~2 segundos hasta reset  
**Respuesta**: `0x0A` (RESP_WATCHDOG_OK)  
**Ejemplo**: `Wire.write(0xFF);`

---

## Tabla Resumen de Comandos

| Rango | Categoría | Descripción |
|-------|-----------|-------------|
| 0xA0-0xAF | 🔴 Seguridad | Comandos críticos de relay |
| 0x00-0x0F | 📊 Lectura | Status y sensores |
| 0x02-0x08 | 💡 Visual | Efectos NeoPixel |
| 0x10-0x2F | 🔊 Audio | PWM/Buzzer/Tonos |
| 0x3A-0x3F | 💾 Config | Flash e I2C |
| 0x40-0x45 | ℹ️ Info | Información dispositivo |
| 0xD8-0xDA | 📈 ADC | Lectura analógica |
| 0xFE-0xFF | 🔄 Reset | Reset del sistema |

---

## Protocolo de Comunicación

### Estructura Básica
```cpp
// Enviar comando
Wire.beginTransmission(device_addr);
Wire.write(comando);
error = Wire.endTransmission();

// Leer respuesta (si aplica)
Wire.requestFrom(device_addr, 1);
if (Wire.available()) {
  uint8_t respuesta = Wire.read();
}
```

### Respuestas con PA4 Digital
Las respuestas incluyen el estado de PA4 en bits superiores [7-4]:
```cpp
uint8_t respuesta = Wire.read();
uint8_t codigo = respuesta & 0x0F;      // Bits [3-0]: Código respuesta
uint8_t pa4 = (respuesta >> 4) & 0x01;  // Bits [7-4]: Estado PA4
```

---

## Consideraciones de Seguridad

### Separación de Comandos Críticos
- **Comandos Relay**: Rango `0xA0-0xAF` (seguridad)
- **Comandos Lectura**: Rango `0x00-0x0F` (status)
- **Separación**: >150 códigos de diferencia entre relay y lectura

### ¿Por qué esta separación?
En buses I2C con 20+ dispositivos, pueden ocurrir errores de transmisión. Si `CMD_PA4_DIGITAL (0x07)` estuviera cerca de `CMD_RELAY_ON (0x01)`, un solo bit corrupto podría:
- Convertir `0x07` → `0x06` (TOGGLE) ⚠️ **APERTURA ACCIDENTAL**
- Convertir `0x07` → `0x01` (RELAY_ON) ⚠️ **APERTURA ACCIDENTAL**

Con la separación actual:
- `CMD_PA4_DIGITAL = 0x07`
- `CMD_RELAY_ON = 0xA1`
- Diferencia: **160 códigos** → Imposible confusión por 1-2 bits corruptos

---

## Configuración I2C

### Parámetros
- **Velocidad**: 100kHz (optimizado para 20+ dispositivos)
- **Addressing**: 7-bit
- **Timeout RX**: 15ms
- **Timeout TX**: 100ms
- **Rango direcciones**: 0x08-0x77

### Direccionamiento Dinámico
Cada dispositivo calcula su dirección I2C desde su UID único:
```cpp
direccion_i2c = 0x08 + (UID % 112);
```
Evita conflictos automáticamente entre dispositivos.

---

## Ejemplos de Uso

### Ejemplo 1: Apertura Segura de Cerradura
```cpp
// Verificar que dispositivo responde
Wire.beginTransmission(addr);
if (Wire.endTransmission() == 0) {
  // Abrir relay
  Wire.beginTransmission(addr);
  Wire.write(CMD_RELAY_ON);  // 0xA1
  Wire.endTransmission();
  
  delay(2000); // Mantener abierto 2 segundos
  
  // Cerrar relay
  Wire.beginTransmission(addr);
  Wire.write(CMD_RELAY_OFF); // 0xA0
  Wire.endTransmission();
}
```

### Ejemplo 2: Leer Sensor y Activar Alerta
```cpp
// Leer estado PA4
Wire.beginTransmission(addr);
Wire.write(CMD_PA4_DIGITAL); // 0x07
Wire.endTransmission();

Wire.requestFrom(addr, 1);
uint8_t resp = Wire.read();
bool sensor = (resp >> 4) & 0x01;

if (sensor == HIGH) {
  // Activar alerta audible
  Wire.beginTransmission(addr);
  Wire.write(CMD_ALERT); // 0x2F - 1500Hz
  Wire.endTransmission();
}
```

### Ejemplo 3: Cambiar Dirección I2C
```cpp
uint8_t old_addr = 0x34;
uint8_t new_addr = 0x50;

// Activar modo cambio dirección
Wire.beginTransmission(old_addr);
Wire.write(CMD_SET_I2C_ADDR); // 0x3D
Wire.endTransmission();

// Enviar nueva dirección
Wire.beginTransmission(old_addr);
Wire.write(new_addr);
Wire.endTransmission();

// A partir de ahora usar new_addr
delay(100);
Wire.beginTransmission(new_addr);
// ...
```

---

## Troubleshooting

### Problema: Dispositivo no responde
**Solución**:
1. Verificar conexiones SDA/SCL
2. Verificar alimentación 3.3V
3. Escanear bus I2C para encontrar dirección
4. Verificar pull-ups (2.2kΩ - 4.7kΩ)

### Problema: Respuestas inconsistentes
**Solución**:
1. Reducir velocidad I2C a 100kHz
2. Aumentar delays entre comandos (10-20ms)
3. Verificar capacitancia del bus (<400pF)
4. Reducir longitud de cables

### Problema: Comandos de relay no funcionan
**Solución**:
1. Verificar que usas nuevos códigos (`0xA0`, `0xA1`, `0xA6`)
2. Actualizar firmware a v6.0.0_lite o superior
3. Verificar respuesta del dispositivo

---

## Historial de Versiones

### v6.0.0_lite (Noviembre 2025)
- ✅ **CRÍTICO**: Comandos relay movidos a rango 0xA0-0xAF (seguridad)
- ✅ Optimización para 20+ dispositivos I2C
- ✅ Módulo PWM completo (buzzer PB6)
- ✅ Timeouts tolerantes (15ms RX, 100ms TX)
- ✅ Recovery robusta con pulsos SCL manuales
- ✅ Documentación completa de comandos I2C

### v5.4.2_lite (Octubre 2024)
- Soporte NeoPixels WS2812B
- Comandos ADC de 12 bits
- Gestión direcciones I2C dinámicas

### v5.4.1 (Septiembre 2024)
- Versión inicial con relay y sensores básicos

---

## Soporte y Contacto

**Firmware**: PY32F003 Locknode v6.0.0_lite  
**MCU**: PY32F003F18 (18KB Flash, 2KB RAM)  
**I2C**: 100kHz, 7-bit addressing  
**Repositorio**: `/locknode_v6_0_0_lite/`

---

**Última actualización**: Noviembre 14, 2025
