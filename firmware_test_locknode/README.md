# 🚀 FIRMWARE v5_4 - I2C ADDRESS MANAGEMENT

## ✨ **NUEVAS FUNCIONALIDADES**

### **🏠 Gestión de Direcciones I2C Avanzada**

**1. Dirección I2C Personalizada:**
- Comando `0x3D`: Establecer nueva dirección I2C
- Se guarda en Flash de forma permanente
- Rango válido: 0x08-0x77 (estándar I2C)

**2. Reset Factory:**
- Comando `0x3E`: Reset a dirección UID por defecto
- Borra configuración personalizada
- Vuelve al comportamiento original

**3. Estado I2C:**
- Comando `0x3F`: Consultar origen de dirección actual
- Respuesta: Flash (0x0F) o UID (0x0A)

### **🔧 Arquitectura Flash Mejorada**

**Mapa de Flash (16KB PY32F003):**
```
0x08003C00 - Flash Data (debug/test)
0x08003E00 - Dirección I2C personalizada
0x08003F00 - Configuración y banderas
```

**Banderas de Configuración:**
- `CONFIG_USE_FLASH_I2C (0x01)`: Usar dirección desde Flash
- `CONFIG_FACTORY_DEFAULT (0x02)`: Configuración factory

### **🎵 Indicación Sonora de Estado**

**Al inicializar:**
- **Flash I2C**: Alto-Bajo-Alto (1500-800-1500 Hz)
- **UID I2C**: Bajo-Alto-Bajo (800-1500-800 Hz)

## 📋 **COMANDOS DISPONIBLES**

### **Comandos I2C Management:**
```
0x3D - SET_I2C_ADDR     : Establecer nueva dirección
0x3E - RESET_FACTORY    : Reset a dirección UID
0x3F - GET_I2C_STATUS   : Estado actual (Flash/UID)
```

### **Respuestas I2C Management:**
```
0x0D - RESP_I2C_ADDR_SET    : Dirección establecida
0x0E - RESP_FACTORY_RESET   : Reset factory OK
0x0F - RESP_I2C_FROM_FLASH  : Usando Flash
0x0A - RESP_I2C_FROM_UID    : Usando UID
```

## 🛠️ **USO DESDE ADMINISTRADOR**

### **Establecer Nueva Dirección:**
```bash
D1:set i2c 0x50          # Nueva dirección 0x50
# Requiere reinicio para aplicar
```

### **Reset Factory:**
```bash
D1:reset factory         # Volver a UID original
# Requiere reinicio para aplicar
```

### **Consultar Estado:**
```bash
D1:i2c status           # Ver origen actual
# Respuesta: Flash o UID
```

## 🔄 **FLUJO DE FUNCIONAMIENTO**

### **1. Inicialización:**
```
1. Leer configuración Flash
2. Si CONFIG_USE_FLASH_I2C:
   - Leer dirección desde Flash
   - Usar si es válida
3. Sino: Usar dirección UID por defecto
4. Configurar I2C con dirección elegida
5. Indicación sonora del origen
```

### **2. Cambio de Dirección:**
```
1. Enviar 0x3D (activar modo)
2. Enviar nueva dirección (0x08-0x77)
3. Se guarda en Flash automáticamente
4. Se activa bandera CONFIG_USE_FLASH_I2C
5. Reiniciar para aplicar cambios
```

### **3. Reset Factory:**
```
1. Enviar 0x3E
2. Se borra configuración Flash
3. Se activa CONFIG_FACTORY_DEFAULT
4. Reiniciar para usar UID original
```

## 💾 **COMPATIBILIDAD**

- **Backward Compatible**: Mantiene todos los comandos v5_3
- **Flash Debug**: Funcionalidad completa 8-bit
- **PWM/ADC/NeoPixel**: Sin cambios
- **PA4 Protocol**: Preservado

## ⚠️ **NOTAS IMPORTANTES**

1. **Reinicio Requerido**: Cambios I2C requieren reinicio
2. **Validación**: Solo direcciones I2C válidas (0x08-0x77)
3. **Persistencia**: Configuración se mantiene tras power-off
4. **Recovery**: Reset factory siempre disponible
5. **Memory Usage**: ~86% Flash, 75% RAM

## 🎯 **CASOS DE USO**

- **Red I2C Organizada**: Direcciones secuenciales (0x10, 0x11, 0x12...)
- **Evitar Conflictos**: Cambiar si otra dispositivo usa misma dirección
- **Debugging**: Direcciones conocidas para testing
- **Production**: Direcciones específicas por lote/aplicación
