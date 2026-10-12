# Hardware - Pinouts and Connections

## Board: Unit Electronics DevLab PY32F003L24D6TR

### Specifications

- **Microcontroller**: PY32F003L24D6TR, ARM Cortex-M0+
- **Maximum clock**: 24 MHz
- **Flash**: 16 KB
- **RAM**: 2 KB
- **Board logic voltage**: 3.3 V
- **Built-in LED**: PB5

### Pinout

Use the labels printed on the DevLab board. The main exposed signals are:

| Board label | Primary use in this project |
|-------------|-----------------------------|
| VCC / GND | Power and common ground |
| PA0 | USART2 TX |
| PA1 | USART2 RX |
| PA2 | ADC_IN2 |
| PB0 / PF2 | GPIO / reset |
| PB5 | Built-in LED, TIM3_CH2 PWM |
| PA13 | SWDIO |
| PA14 | SWCLK |
| PB6 / PA10 | Alternate I2C/GPIO signals |

See the board manufacturer's
[pinout PDF](https://unit-electronics-mx.github.io/unit_devlab_py32f003l24d6tr_development_board/hardware/unit_pinout_v_0_0_1_ue0102_py32f003_en.pdf)
for the physical connector positions.

## WS2812-compatible RGB LED connections on PA0

The standalone `ws2812_pa0` demo uses only PA0 for its data output. Connect
PA0 to the first pixel's DIN and join the grounds. The `rgb_pa0` I2C example
uses the same LED data output plus PA10 and PB6 for I2C on the DevLab board.

| Signal | MCU pin | Alternate function |
|--------|---------|--------------------|
| I2C SDA | PA10 | AF6 |
| I2C SCL | PB6 | AF6 |
| RGB LED DIN | PA0 | TIM1_CH3, AF13 |

Connect PA10 and PB6 to the I2C master with pull-ups to 3.3 V. Connect PA0 to
the first pixel's DIN and join grounds. Power the strip from a supply sized for
its LED count; do not power an 80-pixel strip through the MCU board. If the
strip uses 5 V, use a 3.3 V to 5 V data-level shifter when required by the
LED's input threshold.

On the DevLab's DFN8 package, PA10 and PB6 share physical pads with PA13/SWDIO
and PA14/SWCLK. The firmware configures PA10/PB6 for I2C and leaves PA13/PA14
untouched so SWD remains available for programming. Disconnect the I2C master
from those pads while flashing if it interferes with the SWD signals. To use
PF0/PF1 instead on a package that exposes them, build with
`-DRGB_PA0_I2C_USE_PF0_PF1=ON` and connect SDA to PF0, SCL to PF1.

## Blink Example Connections

### Minimal Setup

| Function | Pin | Port |
|----------|-----|------|
| LED | PB5 | PB5 |
| Ground | GND | - |
| Power | VCC | - |

### Connection Diagram

```
   Unit DevLab PY32F003
   
   ┌─────────────────┐
   │      LED        │
   │       │         │
   │     (~)         │
   │       │         │
   │      220Ω       │
   │       │         │
   │       PB5       │
   │       │         │
   │      GND        │
   └─────────────────┘
```

## UART Example Connections

### Pins

| Function | Pin | Port | Alt Func |
|----------|-----|------|----------|
| TX | PA0 | PA0 | AF9 |
| RX | PA1 | PA1 | AF9 |
| Ground | GND | - | - |
| Power | VCC | - | - |

### To USB-UART Adapter

```
   Unit DevLab          USB-UART Adapter
   
   PA0 (TX) ----[R]---> RX
   PA1 (RX) ----[R]---> TX
   GND      ------------> GND
   VCC      ------------> VCC (optional; use matching voltage)
   
   R = 1kΩ current limiting resistor (recommended)
```

### Serial Terminal Settings

- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None
- **Line Endings**: CR+LF

### Terminal Commands (Linux)

#### minicom

```bash
minicom -b 115200 -o -D /dev/ttyUSB0
```

#### screen

```bash
screen /dev/ttyUSB0 115200
```

#### picocom

```bash
picocom -b 115200 /dev/ttyUSB0
```

#### socat + nc

```bash
socat /dev/ttyUSB0,b115200 STDOUT
```

Exit: Ctrl+A, then X (minicom) or Ctrl+A, then Ctrl+D (screen)

## PWM Example Connections

| Function | Pin | Port | Alt Func |
|----------|-----|------|----------|
| TIM3 channel 2 | PB5 | PB5 | AF1 |
| TIM3 channel 1 | PA2 | PA2 | AF13 |

The example drives the existing PB5 LED with a 1 kHz PWM signal. An
oscilloscope or logic analyzer can also be connected directly to PB5 and GND.

For the `pwm_pa2` example, connect the instrument to PA2 and GND instead. PA2
outputs TIM3 channel 1 PWM on AF13 and cannot simultaneously serve as ADC_IN2.

## ADC Example Connections

| Function | Port | Notes |
|----------|------|-------|
| Analog input | PA2 | ADC_IN2, range 0 V to VDD |
| UART output | PA0 | USART2 TX, AF9, 115200 baud |

Recommended potentiometer connection:

```text
3.3 V ---- potentiometer ---- GND
                 |
                PA2

PA0 ------------------------ USB-UART RX
GND ------------------------ USB-UART GND
```

Do not apply a negative voltage or a voltage above VDD to PA2.

## Debug Probe Connection (Flashing)

### SWD Interface

| Function | Pin | Port |
|----------|-----|------|
| SWDIO | PA13 | - |
| SWCLK | PA14 | - |
| GND | GND | - |
| VCC reference | VCC | - |

### Common Probes

- **OpenDap**: ST-Link clones, RP2040-based probes
- **J-Link**: Proprietary SEGGER probe
- **DAPLink**: ARM-based probes

Connect SWD pins and ground, then:

```bash
pyocd list  # Verify probe detection
```

## Power Supply

### Options

1. **USB 5V** (via debug probe or adapter)
   - Connect 5V to the board VCC input
   - GND to any GND pin

2. **3.3V Regulator**
   - Some boards include LDO
   - Verify output is 3.3V

3. **Direct 3.3V**
   - For bench testing
   - Requires external regulator

### Bypass Capacitors

Recommended for stability:

```
VDD ----||---- GND
     0.1µF
```

## Temperature Range

- **Operating**: 0°C to 70°C
- **Storage**: -40°C to 85°C

## Absolute Maximum Ratings

- **VDD**: 3.6V max
- **GPIO Pins**: 5V tolerant (with current limiting)
- **Dissipation**: 200mW typ

## Next Steps

- [Build firmware](BUILD.md)
- [Flash to device](FLASH.md)
- [Test examples](EXAMPLES.md)
- [Troubleshoot](TROUBLESHOOTING.md)
