# Examples

## Overview

Four example programs are included:

1. **Blink** - Simple LED blinking
2. **UART** - Character echo with startup stabilization
3. **PWM** - LED brightness control with TIM3
4. **ADC** - Analog voltage measurement reported over UART

## Blink Example

### Purpose

Demonstrates basic GPIO control and timing on the PY32F003.

### Source

`examples/blink.c`

### Hardware

- **LED**: PB5 (active high)
- **Clock**: HSI 24 MHz

### Behavior

- Toggles PB5 every 1000 ms using the Cortex-M SysTick timer
- Keeps the LED on for one second and off for one second

### Build

```bash
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=blink ..
cmake --build . -j
```

### Test

1. Flash: `cmake --build . --target flash_reset_blink -j`
2. Observe LED on PB5 blinking steadily

### Code Snippet

```c
while (1) {
    gpio_toggle(GPIOB, GPIO5);
    delay_ms(1000);
}
```

## UART Example

### Purpose

Demonstrates serial communication with echo functionality and startup stabilization.

### Source

`examples/uart.c`

### Hardware

- **UART**: USART2
- **TX**: PA0 (AF9)
- **RX**: PA1 (AF9)
- **Baud Rate**: 115200
- **Format**: 8N1 (8 bits, no parity, 1 stop bit)

### Behavior

1. Prints banner on startup (with 2.4M cycle stabilization delay)
2. Echoes received characters back to sender
3. Converts `\r` to `\r\n` for terminal compatibility
4. Filters RX errors (frame errors, noise, overrun)

### Build

```bash
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=uart ..
cmake --build . -j
```

### Test

1. Flash: `cmake --build . --target flash_reset_uart -j`
2. Open serial terminal:
   ```bash
   screen /dev/ttyUSB0 115200
   # or
   minicom -b 115200 -D /dev/ttyUSB0
   ```
3. You should see:
   ```
   PY32 libopencm3 UART echo
   ```
4. Type characters and see them echoed back

### Expected Output

```
PY32 libopencm3 UART echo
Hello
Hello
World
World
```

### Key Functions

#### `uart_link_stabilize()`

Waits for oscillator/USB adapter to stabilize after reset:
- 2,400,000 cycle delay (~300ms at 8MHz)
- Flushes RX error flags (FE, NE, ORE)
- Clears RXNE (receive ready) flag

```c
static void uart_link_stabilize(void) {
    for (uint32_t i = 0; i < 2400000; i++);
    
    // Clear error flags
    USART2_ICR = USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF;
    USART2_ISR &= ~USART_ISR_RXNE;
}
```

#### `uart_send_blocking()`

Sends a single character:
```c
static void uart_send_blocking(char c) {
    while (!(USART2_ISR & USART_ISR_TXE));
    USART2_TDR = c;
}
```

#### `uart_recv_blocking()`

Receives a single character:
```c
static uint8_t uart_recv_blocking(void) {
    while (!(USART2_ISR & USART_ISR_RXNE));
    uint8_t data = USART2_RDR;
    // Filter frame/noise errors
    if (USART2_ISR & (USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) {
        return 0;
    }
    return data;
}
```

### Startup Sequence

1. Initialize USART2 at 115200 8N1
2. Configure PA0 (TX, AF9) and PA1 (RX, AF9)
3. Call `uart_link_stabilize()` for 300ms
4. Send banner message
5. Enter echo loop

### Common Issues

**Garbled text on startup**: Already handled by `uart_link_stabilize()`. If still occurring:
- Verify UART adapter is connected before powering device
- Check USB cable quality
- Try higher delay in `uart_link_stabilize()` (increase loop count)

**No characters echoed**: 
- Verify TX/RX pins (PA0 and PA1)
- Verify baud rate (115200)
- Check serial cable connections
- Verify common GND between device and adapter

**Only partial characters**: Usually caused by baud rate mismatch or noise on lines

## PWM Example

### Purpose

Generates a 1 kHz PWM signal with TIM3 channel 2 and varies its duty cycle to
create a breathing LED effect.

### Hardware

- **PWM output**: PB5 (TIM3_CH2, AF1)
- **Timer clock**: 8 MHz
- **PWM frequency**: 1 kHz
- **Duty cycle**: ramps between 0% and 100%

### Build and Flash

```bash
cmake --fresh -S . -B build/pwm \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=pwm
cmake --build build/pwm -j
cmake --build build/pwm --target flash-reset -j
```

Observe the LED on PB5 gradually increasing and decreasing in brightness. A
logic analyzer or oscilloscope on PB5 will show a 1 kHz waveform.

### PWM on PA2

The `pwm_pa2` example produces the same 1 kHz duty-cycle ramp on PA2 using
TIM3 channel 1 (AF13). Connect an oscilloscope or logic analyzer between PA2
and GND. PA2 is shared with ADC_IN2, so it cannot be used as an ADC input while
this example is running.

```bash
cmake --fresh -S . -B build/pwm_pa2 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=pwm_pa2
cmake --build build/pwm_pa2 -j
cmake --build build/pwm_pa2 --target flash-reset -j
```

## ADC Example

### Purpose

Samples a voltage on ADC channel 2 and reports both the raw 12-bit value and
the calculated voltage over USART2.

### Hardware

- **Analog input**: PA2 (ADC_IN2, exposed on the DevLab header)
- **Allowed input range**: 0 V to VDD; never apply a negative voltage or more
  than VDD
- **UART TX**: PA0 (AF9), 115200 8N1
- **Reference used by the example**: 3300 mV

Connect a 10 kΩ potentiometer between 3.3 V and GND, with its wiper connected
to PA2. Connect PA0 to the RX input of a USB-UART adapter and join both grounds.

### Build and Flash

```bash
cmake --fresh -S . -B build/adc \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=adc
cmake --build build/adc -j
cmake --build build/adc --target flash-reset -j
```

Open a 115200-baud terminal. The output is updated every 250 ms:

```text
PY32F003 ADC: PA2 / ADC_IN2
ADC=2048/4095, voltage=1650 mV
```

If the board supply is not 3.3 V, change `ADC_REFERENCE_MV` in
`examples/adc.c` to the measured VDD value in millivolts.

## Taller: red de nodos PY32 por I2C

Estos ejemplos convierten cada PY32F003 en un esclavo I2C sencillo. Cada
alumno carga uno de los siguientes firmware y asigna una direccion distinta:

| Funcion | Firmware PY32 | Tipo |
|---------|---------------|------|
| ADC / TEMT6000 | `examples/i2c_adc.c` | `0x01` |
| Salida digital | `examples/i2c_on_off.c` | `0x02` |
| PWM | `examples/i2c_pwm.c` | `0x03` |

En el firmware elegido se cambia solamente la direccion de 7 bits:

```c
#define I2C_SLAVE_ADDRESS 0x20U
```

Las direcciones validas van de `0x08` a `0x77` y no deben repetirse en la
misma red. En `i2c_adc.c`, `DEVICE_MODEL` permite identificar el sensor:
`0x01` corresponde al TEMT6000 y `0x00` a un sensor analogico generico. La
lectura ADC siempre se entrega como valor crudo de 12 bits (`0..4095`).

### Conexiones

| Senal | PY32F003 |
|-------|----------|
| SCL | PB6 (AF6) |
| SDA | PA10 (AF6) |
| PWM / salida digital | PB5 |
| Entrada ADC | PA2 / ADC_IN2 |

El bus trabaja a 100 kHz. SDA y SCL requieren resistencias pull-up y todos los
dispositivos deben compartir GND. Los ejemplos configuran solamente PB6/SCL y
PA10/SDA: PA13/SWDIO y PA14/SWCLK permanecen disponibles para programacion.

### Protocolo minimo

Cada esclavo procesa los comandos con un `switch` en `process_i2c_write()`.
El maestro escribe el comando, termina la transmision y solicita la respuesta.

| Comando | Nodo | Operacion | Respuesta |
|---------|------|-----------|-----------|
| `0x00` | Todos | Identificacion | 2 bytes: tipo, modelo |
| `0x01` | ADC | Leer PA2 | 2 bytes: ADC LSB, ADC MSB |
| `0x02, 0` o `0x02, 1` | ON/OFF | Cambiar salida | 1 byte: estado actual |
| `0x03, porcentaje` | PWM | Cambiar ciclo util `0..100` | 1 byte: porcentaje actual |

### Compilar y programar

```bash
# Sustituye i2c_adc por i2c_on_off o i2c_pwm segun el nodo.
cmake --fresh -S . -B build/i2c_adc \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=i2c_adc
cmake --build build/i2c_adc --target flash-reset -j
```

### Maestro Arduino para la red

El maestro esta en
`examples/arduino_master/i2c_network/i2c_network.ino`. Usa solamente `Wire`,
sin DevLabDDP ni bibliotecas externas. Abre el monitor serie a 115200 baudios
con terminacion `Newline`.

```text
scan
automap
map
clear map
adc
adc 1
adc 0x20
on 1
on 2 0
on 0x21 1
pwm 50
pwm 3 75
pwm 0x22 25
```

`scan` muestra todas las direcciones presentes y `automap` consulta el comando
`0x00` para clasificar los nodos. Los numeros decimales usados como dispositivo
son indices de `map`; una direccion directa se escribe en hexadecimal. Sin
indicar dispositivo, `adc` lee todos los nodos ADC, `on 1` controla el primer
nodo ON/OFF y `pwm 50` controla el primer nodo PWM.

Los maestros Arduino individuales permanecen en `examples/arduino_master/`
para probar cada funcion por separado.

## Next Steps

- [Flash examples to device](FLASH.md)
- [View hardware connections](HARDWARE.md)
- [Troubleshoot issues](TROUBLESHOOTING.md)
