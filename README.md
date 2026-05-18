#  Locknode V8 - PY32F003 Firmware Project

[![Platform](https://img.shields.io/badge/Platform-PY32F003-blue.svg)](https://www.puyasemi.com/)
[![I2C](https://img.shields.io/badge/Interface-I2C-green.svg)](https://en.wikipedia.org/wiki/I%C2%B2C)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Firmware avanzado para microcontrolador **PY32F003** con gestión I2C inteligente, control de relés, NeoPixels, ADC y GPIO. Diseñado para sistemas IoT distribuidos con direccionamiento dinámico.

---

##  Tabla de Contenidos

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

## Características

### **Core Features**
- **Gestión I2C Dinámica**: Cambio de dirección I2C en tiempo de ejecución con persistencia en Flash
- **Control de Relé**: Comandos ON/OFF y toggle con protección temporal
- **NeoPixels RGB**: Control de LEDs direccionables (WS2812B compatible)
- **Lectura ADC**: ADC de 12 bits en PA0 con respuesta I2C
- **GPIO Digital**: Lectura de entrada PA4 con debounce por hardware
- **UID Único**: Dirección I2C automática basada en UID del chip

### **Advanced Features**
- **Factory Reset**: Restaurar dirección UID por defecto
- **Estado I2C**: Consultar origen de dirección (Flash vs UID)
- **Indicación Sonora**: Feedback audible del estado de inicialización
- **Watchdog Reset**: Reset por comando con timeout seguro
- **Flash Protegida**: Manejo robusto de escritura en Flash con verificación
