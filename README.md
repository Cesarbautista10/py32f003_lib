# 🔐 Locknode V8 - PY32F003 Firmware Project

[![Platform](https://img.shields.io/badge/Platform-PY32F003-blue.svg)](https://www.puyasemi.com/)
[![I2C](https://img.shields.io/badge/Interface-I2C-green.svg)](https://en.wikipedia.org/wiki/I%C2%B2C)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Firmware avanzado para microcontrolador **PY32F003** con gestión I2C inteligente, control de relés, NeoPixels, ADC y GPIO. Diseñado para sistemas IoT distribuidos con direccionamiento dinámico.

---

## 📋 Tabla de Contenidos

- [Características](#-características)
- [Versiones de Firmware](#-versiones-de-firmware)
- [Estructura del Proyecto](#-estructura-del-proyecto)
- [Herramientas de Administración](#-herramientas-de-administración)
- [Instalación](#-instalación)
- [Compilación](#-compilación)
- [Flasheo](#-flasheo)
- [Comandos I2C](#-comandos-i2c)
- [Hardware Soportado](#-hardware-soportado)
- [Contribuir](#-contribuir)

---

## ✨ Características

### 🔧 **Core Features**
- **Gestión I2C Dinámica**: Cambio de dirección I2C en tiempo de ejecución con persistencia en Flash
- **Control de Relé**: Comandos ON/OFF y toggle con protección temporal
- **NeoPixels RGB**: Control de LEDs direccionables (WS2812B compatible)
- **Lectura ADC**: ADC de 12 bits en PA0 con respuesta I2C
- **GPIO Digital**: Lectura de entrada PA4 con debounce por hardware
- **UID Único**: Dirección I2C automática basada en UID del chip

### 🎯 **Advanced Features**
- **Factory Reset**: Restaurar dirección UID por defecto
- **Estado I2C**: Consultar origen de dirección (Flash vs UID)
- **Indicación Sonora**: Feedback audible del estado de inicialización
- **Watchdog Reset**: Reset por comando con timeout seguro
- **Flash Protegida**: Manejo robusto de escritura en Flash con verificación

---

## 📦 Versiones de Firmware

### **firmware_v5_4_2** (Recomendado) ✅
- **Fix crítico**: Aislamiento de lectura PA4 solo para comando específico
- **Mejora I2C**: Eliminación de conflictos en respuestas mixtas
- **Estabilidad**: Bus I2C más robusto sin lock-ups
- **Arquitectura**: Separación clara entre comandos de escritura y lectura

### **firmware_v5_4_1**
- Gestión completa de direcciones I2C
- Comandos de control (relay, neo, ADC, PA4)
- Persistencia en Flash
- Indicación sonora de estado

### **firmware_v5_4_lite**
- Versión compacta con funciones esenciales
- Ideal para memoria reducida
- GPIO simplificado

### **blink**
- Firmware de prueba básico
- Validación de hardware
- Debugging inicial

---

## 📁 Estructura del Proyecto

```
locknodev8/
├── firmware_v5_4_2/          # Firmware principal (recomendado)
│   ├── main.c                # Lógica principal e I2C
│   ├── main.h                # Definiciones y comandos
│   ├── adc_control.c/h       # Control ADC PA0
│   ├── pwm_control.c/h       # PWM para buzzer/NeoPixels
│   ├── neo_spi.c/h           # Driver NeoPixels WS2812B
│   ├── systemConfig.c/h      # Clock y periféricos
│   ├── py32f0xx_it.c/h       # Interrupciones
│   ├── Makefile              # Compilación específica
│   └── README.md             # Documentación firmware
│
├── admin5/                   # Herramientas de gestión
│   ├── admin_gr.ino          # Manager completo con test continuo
│   └── admin_ir.ino          # Manager de direcciones I2C
│
├── Libraries/                # HAL Drivers PY32F0xx
│   ├── CMSIS/                # Core ARM Cortex-M0+
│   ├── PY32F0xx_HAL_Driver/  # HAL periféricos
│   └── LDScripts/            # Linker scripts
│
├── Build/                    # Salida de compilación
│   ├── firmware_*.bin        # Binarios finales
│   ├── firmware_*.hex        # Hex para programadores
│   └── firmware_*.elf        # Símbolos debug
│
├── build_tools/              # Scripts automatización
│   ├── master_build.sh       # Build multi-proyecto
│   └── configs/              # Configuraciones build
│
├── Misc/                     # Utilidades
│   ├── pyocd.yaml            # Config PyOCD
│   ├── py32f003.cfg          # OpenOCD config
│   └── Flash/                # Herramientas flash
│
├── Makefile                  # Build principal
├── rules.mk                  # Reglas comunes
└── README.md                 # Este archivo
```

---

## 🛠️ Herramientas de Administración

### **admin_gr.ino** - Manager Completo
Herramienta Arduino para ESP32 con funciones avanzadas:

#### **Funciones Principales:**
- ✅ **Escaneo I2C**: Detecta dispositivos PY32F003 (0x08-0x77)
- ✅ **Cambio de Dirección**: Comando interactivo `c OLD NEW`
- ✅ **Factory Reset**: Restaurar UID con comando `f ADDR`
- ✅ **Test Continuo**: 3 modos de prueba automatizada
- ✅ **Estadísticas**: Tracking de éxito/fallo por dispositivo
- ✅ **Comandos Dispositivo**: relay, neo, adc, pa4, toggle

#### **Modos de Test Continuo:**
```bash
start pa4      # Solo lectura PA4 (no invasivo)
start toggle   # Solo toggle de relé
start both     # Toggle + lectura PA4 con delay
```

#### **Configuración I2C:**
- **SDA**: GPIO21 (ESP32 clásico) o GPIO6 (ESP32-C3)
- **SCL**: GPIO22 (ESP32 clásico) o GPIO7 (ESP32-C3)
- **Clock**: 100kHz (estabilidad óptima)
- **Timeout**: 100ms

### **admin_ir.ino** - Manager Direcciones
Versión simplificada enfocada en gestión de direcciones I2C.

---

## 🚀 Instalación

### **Requisitos del Sistema**

#### **Hardware:**
- PY32F003 (Cortex-M0+, 16KB Flash, 2KB RAM)
- Programador: J-Link, ST-Link V2, PyOCD compatible
- ESP32 o ESP32-C3 (para herramientas admin)

#### **Software:**
```bash
# Toolchain ARM
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi

# PyOCD (recomendado)
pip install pyocd

# OpenOCD (alternativo)
sudo apt install openocd

# Arduino IDE (para admin tools)
# Descargar desde: https://www.arduino.cc/en/software
```

### **Clonar Repositorio**
```bash
git clone ssh://git@git.uelectronics.com:2222/i2d-swe/unit_py32_locknode_firmware.git
cd unit_py32_locknode_firmware
```

---

## 🔨 Compilación

### **Opción 1: Compilar Firmware Específico**
```bash
# Compilar firmware v5.4.2 (recomendado)
make PROJECT=firmware_v5_4_2

# Compilar firmware v5.4.1
make PROJECT=firmware_v5_4_1

# Limpiar build
make PROJECT=firmware_v5_4_2 clean
```

### **Opción 2: Build Master (Todos los Proyectos)**
```bash
cd build_tools
./master_build.sh
```

### **Salida:**
```
Build/
├── firmware_v5_4_2.bin    # Binario listo para flash
├── firmware_v5_4_2.hex    # Formato Intel HEX
├── firmware_v5_4_2.elf    # Símbolos debug
└── firmware_v5_4_2.map    # Mapa de memoria
```

---

## ⚡ Flasheo

### **Método 1: PyOCD (Recomendado)**
```bash
# Flash automático con make
make PROJECT=firmware_v5_4_2 flash

# Flash manual
pyocd flash -t py32f003 Build/firmware_v5_4_2.bin
```

### **Método 2: OpenOCD**
```bash
# Usar configuración personalizada
openocd -f Misc/py32f003.cfg \
        -c "program Build/firmware_v5_4_2.bin 0x08000000 verify reset exit"
```

### **Método 3: J-Link**
```bash
JLinkExe -device PY32F003 -if SWD -speed 4000 \
         -CommandFile Misc/jlink-script
```

### **Script de Flash Rápido**
```bash
# Compilar + Flash en un comando
./build_and_flash.sh firmware_v5_4_2
```

---

## 📡 Comandos I2C

### **Gestión de Direcciones (0x3D-0x3F)**

#### **0x3D - SET_I2C_ADDR**: Establecer Nueva Dirección
```c
// Secuencia de cambio 0x20 -> 0x30
Wire.beginTransmission(0x20);
Wire.write(0x3D);               // Activar modo cambio
Wire.endTransmission();
delay(100);

Wire.beginTransmission(0x20);
Wire.write(0x30);               // Nueva dirección
Wire.endTransmission();
delay(200);

Wire.beginTransmission(0x20);
Wire.write(0xFE);               // Reset para aplicar
Wire.endTransmission();
delay(3000);                    // Esperar reinicio
```

#### **0x3E - RESET_FACTORY**: Restaurar UID
```c
Wire.beginTransmission(address);
Wire.write(0x3E);
Wire.endTransmission();
// Dirección cambiará al UID único del chip
```

#### **0x3F - GET_I2C_STATUS**: Consultar Estado
```c
Wire.beginTransmission(address);
Wire.write(0x3F);
Wire.endTransmission();

Wire.requestFrom(address, 1);
uint8_t status = Wire.read();
// 0x0F = Flash, 0x0A = UID
```

### **Control de Relé (0x00-0x01, 0x06)**

```c
Wire.write(0x00);  // Relé OFF
Wire.write(0x01);  // Relé ON
Wire.write(0x06);  // Toggle (pulso 25ms)
```

### **NeoPixels (0x02-0x05)**

```c
Wire.write(0x02);  // Rojo
Wire.write(0x03);  // Verde
Wire.write(0x04);  // Azul
Wire.write(0x05);  // OFF
```

### **Lectura ADC (0xD8-0xD9)**

```c
// Leer ADC de 12 bits
Wire.write(0xD8);              // HSB (bits 11-8)
Wire.requestFrom(addr, 1);
uint16_t hsb = Wire.read() & 0x0F;

Wire.write(0xD9);              // LSB (bits 7-0)
Wire.requestFrom(addr, 1);
uint16_t lsb = Wire.read();

uint16_t adc_value = (hsb << 8) | lsb;
```

### **GPIO PA4 (0x07)**

```c
Wire.write(0x07);
Wire.requestFrom(addr, 1);
uint8_t response = Wire.read();
bool pa4_state = (response & 0xF0) != 0;  // HIGH/LOW
```

### **Reset (0xFE-0xFF)**

```c
Wire.write(0xFE);  // Reset inmediato
Wire.write(0xFF);  // Reset por watchdog
```

---

## 🔌 Hardware Soportado

### **Pinout PY32F003**

| Pin | Función        | Descripción                    |
|-----|----------------|--------------------------------|
| PA0 | ADC_IN0        | Entrada analógica 12-bit       |
| PA4 | GPIO_INPUT     | Entrada digital con pull-up    |
| PB5 | GPIO_OUTPUT    | Control relé                   |
| PB6 | PWM/SPI        | NeoPixels (WS2812B)            |
| PA9 | I2C_SCL        | Clock I2C                      |
| PA10| I2C_SDA        | Data I2C                       |

### **Periféricos Utilizados**
- **I2C1**: Comunicación slave (hasta 400kHz)
- **TIM1**: PWM buzzer indicador
- **TIM3**: PWM para NeoPixels (emulación SPI)
- **ADC**: 12-bit, conversión single-shot
- **FLASH**: Escritura/lectura configuración

---

## 🧪 Testing

### **Test Manual con admin_gr**
```bash
# Cargar admin_gr.ino en ESP32
# Abrir Serial Monitor (115200 baud)

s                   # Escanear bus I2C
start pa4           # Test continuo lectura
stop                # Detener test
stat                # Ver estadísticas
test 32             # Test completo dispositivo 0x20
```

### **Validación de Comandos**
```bash
relay 32 on         # Encender relé
relay 32 off        # Apagar relé
toggle 32           # Toggle pulso
neo 32 red          # NeoPixels rojos
pa4 32              # Leer PA4
adc 32              # Leer ADC
```

---

## 🐛 Troubleshooting

### **Problema: Bus I2C se congela**
**Solución**: Usar firmware v5.4.2 con aislamiento PA4
```bash
make PROJECT=firmware_v5_4_2 clean
make PROJECT=firmware_v5_4_2 flash
```

### **Problema: Dispositivo no responde después de cambio**
**Solución**: Factory reset
```bash
# En admin_gr
f OLD_ADDRESS
s                   # Buscar nueva dirección UID
```

### **Problema: Timeouts I2C frecuentes**
**Solución**: Reducir clock I2C a 50kHz en ESP32
```cpp
Wire.setClock(50000);
Wire.setTimeout(500);
```

---

## 📝 Changelog

### v5.4.2 (2025-10-29) - STABLE ✅
- **[FIX]** Aislamiento lectura PA4 solo para comando 0x07
- **[FIX]** Eliminación de respuestas mixtas I2C
- **[IMPROVE]** Estabilidad bus I2C aumentada
- **[FEATURE]** admin_gr con 3 modos de test continuo

### v5.4.1 (2025-10-28)
- **[FEATURE]** Gestión direcciones I2C dinámica
- **[FEATURE]** Factory reset a UID
- **[FEATURE]** Indicación sonora estado
- **[FEATURE]** Comandos ADC y PA4

---

## 👥 Contribuir

### **Reportar Issues**
Usa el [issue tracker](https://git.uelectronics.com/i2d-swe/unit_py32_locknode_firmware/-/issues) para:
- 🐛 Bugs
- ✨ Feature requests
- 📚 Mejoras documentación

### **Pull Requests**
1. Fork el proyecto
2. Crea tu feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit tus cambios (`git commit -m 'Add: AmazingFeature'`)
4. Push a la branch (`git push origin feature/AmazingFeature`)
5. Abre un Pull Request

### **Estilo de Código**
- Seguir estilo HAL de Puya
- Comentarios en español/inglés
- Documentar funciones complejas

---

## 📄 Licencia

Este proyecto está bajo la Licencia MIT - ver archivo [LICENSE](LICENSE) para detalles.

---

## 🙏 Agradecimientos

- **Puya Semiconductor** - PY32F0xx HAL Drivers
- **ARM** - CMSIS Core
- **Comunidad I2C** - Especificaciones y best practices

---

## 📧 Contacto

**UNIT Electronics**
- Website: [uelectronics.com](https://uelectronics.com)
- Email: soporte@uelectronics.com
- GitLab: [i2d-swe](https://git.uelectronics.com/i2d-swe)

---

**Desarrollado con ❤️ para la comunidad maker**
