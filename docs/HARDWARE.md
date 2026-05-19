# Hardware - Pinouts and Connections

## Device: PY32F003X4

### Specifications

- **Microcontroller**: ARM Cortex-M0+ 
- **Clock**: 8 MHz HSI (internal oscillator)
- **Flash**: 16 KB
- **RAM**: 2 KB
- **Pins**: 20 (LQFP20)
- **Package**: LQFP20

### Pinout

```
          VDD
           |
    SWCLK--1  20--VSS
    SWDIO--2  19--PA10
     PA13--3  18--PA9
     PA14--4  17--PA8
      PA7--5  16--PA2
      PA6--6  15--PA3
      PA5--7  14--PA4
      PA4--8  13--PA1 (RX)
      PA3--9  12--PA0 (TX)
      PA2-10  11--PB5 (LED)
      PA1-11  10--GND
      PA0-12   9--PB0
      GND-13   8--PB1
      PB5-14   7--PA15
      GND-15   6--PA14
      PB0-16   5--PA13
      PB1-17   4--PA12
     PA15-18   3--PA11
     PA14-19   2--VSS
     PA13-20   1--VDD
```

## Blink Example Connections

### Minimal Setup

| Function | Pin | Port |
|----------|-----|------|
| LED | 14 | PB5 |
| GND | 15, 20, 13 | - |
| VDD | 1 | - |

### Connection Diagram

```
   PY32F003X4 (LQFP20)
   
   ┌─────────────────┐
   │      LED        │
   │       │         │
   │     (~)         │
   │       │         │
   │      220Ω       │
   │       │         │
   │      PB5 (14)   │
   │       │         │
   │      GND        │
   └─────────────────┘
```

## UART Example Connections

### Pins

| Function | Pin | Port | Alt Func |
|----------|-----|------|----------|
| TX | 12 | PA0 | AF9 |
| RX | 11 | PA1 | AF9 |
| GND | 13, 15, 20 | - | - |
| VDD | 1 | - | - |

### To USB-UART Adapter

```
   PY32F003X4          USB-UART Adapter
   
   PA0 (TX) ----[R]---> RX
   PA1 (RX) ----[R]---> TX
   GND      ------------> GND
   VDD      ------------> VCC (optional)
   
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

## Debug Probe Connection (Flashing)

### SWD Interface

| Function | Pin | Port |
|----------|-----|------|
| SWDIO | 2 | - |
| SWCLK | 1 | - |
| GND | 13, 15, 20 | - |
| VDD | Optional | - |

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
   - Connect 5V to VDD
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
