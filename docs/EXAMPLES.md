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
| ADC generico | `examples/i2c_adc.c` | `0x01` |
| Salida digital | `examples/i2c_on_off.c` | `0x02` |
| PWM | `examples/i2c_pwm.c` | `0x03` |
| NeoPixel WS2812 en PA0 | `examples/rgb_pa0.c` | `0x04` |

En los ejemplos ADC, ON/OFF y PWM se cambia la direccion de 7 bits en el
archivo C:

```c
#define I2C_SLAVE_ADDRESS 0x20U
```

Para el nodo RGB se usa `-DRGB_PA0_I2C_ADDRESS=0x20` al configurar CMake.

Las direcciones validas van de `0x08` a `0x77` y no deben repetirse en la
misma red. Por ejemplo, usa ADC `0x20`, ON/OFF `0x21`, PWM `0x22` y RGB `0x23`.
Si dos nodos conservan la direccion predeterminada `0x20`, `scan` no puede
distinguirlos y `automap` puede recibir respuestas mezcladas. En `i2c_adc.c`,
`DEVICE_MODEL` del ejemplo ADC vale `0x00`: es una demostracion de entrada
analogica y no presupone ningun sensor. La lectura siempre se entrega como
valor crudo de 12 bits (`0..4095`).

### Conexiones

| Senal | PY32F003 |
|-------|----------|
| SCL | PB6 (AF6) |
| SDA | PA10 (AF6) |
| PWM / salida digital | PB5 |
| Entrada ADC | PA2 / ADC_IN2 |

El bus trabaja a 100 kHz. SDA y SCL requieren resistencias pull-up y todos los
dispositivos deben compartir GND. En el encapsulado DFN8, PB6 y PA10 comparten
pads con PA14/SWCLK y PA13/SWDIO. Los ejemplos configuran PB6 y PA10 para I2C
sin cambiar PA13 ni PA14, conservando la interfaz SWD para programacion.

### Protocolo minimo

Cada esclavo procesa los comandos con un `switch` en `process_i2c_write()`.
El maestro escribe el comando, termina la transmision y solicita la respuesta.

| Comando | Nodo | Operacion | Respuesta |
|---------|------|-----------|-----------|
| `0x00` | Todos | Identificacion | 2 bytes: tipo, modelo |
| `0x01` | ADC | Leer PA2 | 2 bytes: ADC LSB, ADC MSB |
| `0x02, 0` o `0x02, 1` | ON/OFF | Cambiar salida | 1 byte: estado actual |
| `0x03, porcentaje` | PWM | Cambiar ciclo util `0..100` | 1 byte: porcentaje actual |
| `0x02`/`0x03`/`0x04`/`0x05`/`0x08` | RGB | Rojo/verde/azul/apagar/blanco | 1 byte: comando |
| `0x50, R, G, B` | RGB | Llenar y mostrar color | `0x50` |
| `0x51, indice, R, G, B` | RGB | Cambiar un pixel en memoria | `0x51` |
| `0x52` | RGB | Mostrar pixeles en memoria | `0x52` |

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

Compila y carga el maestro con el FQBN y puerto de tu placa. Por ejemplo, para
un ESP32 conectado en `/dev/ttyUSB0`:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-path build/arduino_i2c_network examples/arduino_master/i2c_network
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 \
  --build-path build/arduino_i2c_network examples/arduino_master/i2c_network
```

```text
scan
probe 0x20
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
red
rgb 32 0 16
pixel 0 255 0 0
show
rgb 0x23 16 32 64
```

`scan` muestra todas las direcciones presentes y `automap` consulta el comando
`0x00` para clasificar los nodos, incluido NeoPixel PA0 (tipo `0x04`). Ambos hacen al
menos dos pasadas y hasta cuatro si al inicio no aparecen nodos o hay errores
de bus. Cuando una pasada no encuentra nodos, el maestro reinicia I2C y espera
500 ms antes de reintentar; si se repiten los errores, corta la pasada e
informa el problema.
`probe 0x20` comprueba una sola direccion e indica ACK, NACK o el estado de
error de `Wire`, util para revisar el nodo RGB sin barrer toda la red.
Si `automap` muestra `Unknown identity at 0x20`, los dos bytes impresos son la
respuesta al comando `0x00`. El firmware NeoPixel PA0 actual responde
`0x04 0x01`: tipo `0x04`, modelo PA0 `0x01`. El maestro tambien reconoce el
firmware anterior con respuesta `0x04 0x00` y lo muestra como
`NEOPIXEL PA0 (legacy)`. Si el firmware instalado aun no implementa `0x00`,
`map add 0x20 neopixel` registra ese nodo manualmente sin enviar comandos al
esclavo; cambia `0x20` por su direccion real.
Los numeros decimales usados como dispositivo
son indices de `map`; una direccion directa se escribe en hexadecimal. Sin
indicar dispositivo, `adc` lee todos los nodos ADC, `on 1` controla el primer
nodo ON/OFF, `pwm 50` controla el primer nodo PWM y los comandos RGB controlan
el primer nodo RGB. `pixel` requiere `show` para enviar los cambios a la tira.
`automap` conserva los nodos que ya conoce aunque un barrido posterior sea
parcial; `clear map` permite vaciar la lista antes de volver a descubrirla
despues de cambiar direcciones.
Vuelve a grabar `rgb_pa0` antes de usar `automap` para incluir su identificacion.

Los maestros Arduino individuales permanecen en `examples/arduino_master/`
para probar cada funcion por separado.

## ws2812_pa0: standalone RGB LED demo

`ws2812_pa0` drives a WS2812-compatible strip directly from PA0 (TIM1_CH3,
AF13). It cycles through dim red, green, blue, and white, then moves one white
pixel along the strip. It needs no I2C connection or master. Connect PA0 to the
first LED's DIN, share ground, and power the strip from a suitable supply. The default is
80 LEDs; `WS2812_PA0_LED_COUNT` accepts 1 to 200.

```bash
cmake --fresh -S . -B build/ws2812_pa0 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=ws2812_pa0 \
  -DWS2812_PA0_LED_COUNT=80
cmake --build build/ws2812_pa0 -j
cmake --build build/ws2812_pa0 --target flash-reset -j
```

## rgb_pa0: WS2812-compatible RGB LEDs over I2C

`rgb_pa0` is a libopencm3 port of LockNode's RGB LED control. It
keeps the one-byte color commands used by `DevLab_MapController` and adds
direct RGB commands. The firmware drives 80 GRB WS2812-compatible pixels on
PA0 through TIM1_CH3 with DMA at 800 kHz. The default colors use intensity
32/255. `RGB_PA0_PIXEL_COUNT` supports 1 to 200 LEDs to leave stack space in
the MCU's 2 KB RAM.

Default DevLab wiring: PB6 is I2C SCL, PA10 is I2C SDA (both AF6), and PA0 is
the RGB LED data output (TIM1_CH3, AF13). The slave address is `0x20` by
default. Use external I2C pull-ups to 3.3 V and share ground with the master.
On the DFN8 package, PA10/PB6 share pads with SWD. The firmware leaves
PA13/SWDIO and PA14/SWCLK untouched for programming. For a package with
PF0/PF1 available, build with
`-DRGB_PA0_I2C_USE_PF0_PF1=ON` and connect SDA/SCL to PF0/PF1 instead.

| I2C write | Action | Readback |
|-----------|--------|-------------------|
| `00` | Identify as NeoPixel WS2812 on PA0 | `04 01` (two bytes) |
| `02` | Fill red | `02` |
| `03` | Fill green | `03` |
| `04` | Fill blue | `04` |
| `05` | Turn off | `05` |
| `08` | Fill white | `08` |
| `50 R G B` | Fill with custom RGB color | `50` |
| `51 index R G B` | Set one zero-based pixel in the buffer | `51` |
| `52` | Show the buffered pixels | `52` |

### Arduino RGB controller

Flash `examples/arduino_master/i2c_rgb/i2c_rgb.ino` to an Arduino-compatible
I2C master. It sends the commands above to `rgb_pa0` at address `0x20` and
checks the one-byte response. On ESP32 it uses GPIO6 for SDA and GPIO7 for SCL,
matching the ADC and ON/OFF masters. Other boards use their `Wire` defaults;
edit `beginI2C()` for a different pin pair.
Connect the master's SDA to PA10 and SCL to PB6, add pull-ups to 3.3 V, and
share ground.

Find your board's FQBN and serial port with `arduino-cli board list`, then
compile and upload the controller. For an Arduino Uno, use
`RGB_BOARD_FQBN=arduino:avr:uno`; replace the example port as needed.

```bash
RGB_BOARD_FQBN=esp32:esp32:esp32
RGB_SERIAL_PORT=/dev/ttyUSB0
arduino-cli compile --fqbn "$RGB_BOARD_FQBN" \
  --build-path build/arduino_i2c_rgb examples/arduino_master/i2c_rgb
arduino-cli upload --fqbn "$RGB_BOARD_FQBN" -p "$RGB_SERIAL_PORT" \
  --build-path build/arduino_i2c_rgb examples/arduino_master/i2c_rgb
```

Open the serial monitor at 115200 baud with newline termination. For example:

```text
rgb 32 0 16
pixel 0 255 0 0
pixel 1 0 0 255
show
red
off
demo
```

`rgb` updates every LED immediately. `pixel` edits the buffer; `show` sends
the edited buffer to the strip. Set `SLAVE_ADDRESS` in the sketch to the same
value as `RGB_PA0_I2C_ADDRESS` in the PY32 firmware.

Invalid commands, lengths, or pixel indices return `FF`. Each write must end
with STOP; wait at least 4 ms for the default 80 pixels, then request one byte
to read its result. For other pixel counts, allow about 1.25 us per data bit
plus 0.7 ms for the initial and final low intervals.
`51` changes the buffer only, so send `52` after setting one or more pixels.
The other color commands update the strip immediately. RGB bytes range from
`00` to `FF`; the driver converts them to the GRB order used by the LEDs.
LockNode's relay, ADC, PWM, flash color-slot, and dynamic address commands are
outside this example.

```bash
cmake --fresh -S . -B build/rgb_pa0 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=rgb_pa0 \
  -DRGB_PA0_I2C_ADDRESS=0x20 \
  -DRGB_PA0_PIXEL_COUNT=80 \
  -DRGB_PA0_I2C_USE_PF0_PF1=OFF
cmake --build build/rgb_pa0 -j
cmake --build build/rgb_pa0 --target flash-reset -j
```

Set a different `RGB_PA0_I2C_ADDRESS` for each node on the same bus. The
firmware starts with all pixels off. Its five color commands use LockNode's
original byte values, so existing masters can send them unchanged. The new
`50`-`52` commands leave LockNode's `40`-`46` device-info and color-slot range
available for future ports.

## Next Steps

- [Flash examples to device](FLASH.md)
- [View hardware connections](HARDWARE.md)
- [Troubleshoot issues](TROUBLESHOOTING.md)
