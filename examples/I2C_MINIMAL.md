# Ejemplos I2C esclavo mínimos

Estos ejemplos son para el **PY32F003**. El maestro de prueba es Arduino y usa
únicamente `Wire`; no requiere DevLabDDP ni otra biblioteca externa.

## Conexiones

| Señal | PY32F003 |
|---|---|
| SCL | PB6 (AF6) |
| SDA | PA10 (AF6) |
| PWM / salida digital | PB5 |
| Entrada ADC TEMT6000 | PA2 / ADC_IN2 |

El bus trabaja a 100 kHz. SDA y SCL necesitan resistencias pull-up. Todos los
ejemplos usan inicialmente la dirección I2C de 7 bits `0x20`. Para cambiarla,
edita `I2C_SLAVE_ADDRESS` al principio del archivo C y `SLAVE_ADDRESS` en el
sketch Arduino correspondiente.

## Protocolo pequeño

Los tres ejemplos usan el registro `0x01` y un `switch` en
`process_i2c_write()`.

| Esclavo PY32 | Escritura del maestro | Respuesta |
|---|---|---|
| `i2c_adc.c` | `0x01` | 2 bytes: ADC LSB, ADC MSB |
| `i2c_on_off.c` | `0x01, 0` o `0x01, 1` | 1 byte: estado |
| `i2c_pwm.c` | `0x01, porcentaje` | 1 byte: porcentaje 0..100 |

Para leer, el maestro primero termina la escritura del registro, espera 1 ms y
después solicita la respuesta. Esto deja el flujo fácil de observar con un
analizador lógico.

## Compilar

Ejemplo para ADC:

```bash
cmake --fresh -S . -B build/i2c_adc \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=i2c_adc
cmake --build build/i2c_adc -j
```

Cambia `i2c_adc` por `i2c_on_off` o `i2c_pwm` para compilar los otros
esclavos. Los maestros están en `examples/arduino_master/`.

## Ejemplos básicos incluidos

| Función | Archivo |
|---|---|
| ADC por PA2 | `adc.c` |
| Blink | `blink.c` |
| PWM por PA0 | `pwm_pa0.c` |
| PWM por PB5 | `pwm.c` |
| UART por PA0/PA1 | `uart.c` |
| I2C PWM mínimo | `i2c_pwm.c` |
| I2C ON/OFF mínimo | `i2c_on_off.c` |
| I2C ADC mínimo | `i2c_adc.c` |
