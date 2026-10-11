# PY32F003 + libopencm3

This project supports building and flashing PY32F003 firmware with a CMake workflow similar to pico-sdk.

## Quick Links

- **Full documentation**: See [docs/INDEX.md](docs/INDEX.md)
- **Getting started**: [Setup](docs/SETUP.md) → [Build](docs/BUILD.md) → [Flash](docs/FLASH.md)
- **Examples**: [Blink, UART, PWM, and ADC](docs/EXAMPLES.md)
- **Hardware setup**: [Pinouts and connections](docs/HARDWARE.md)
- **Stuck?**: [Troubleshooting](docs/TROUBLESHOOTING.md)

## Requirements

- arm-none-eabi-gcc
- cmake 3.16+
- make or gmake
- pyocd

## Project Layout

- examples/
  - blink.c
  - uart.c
  - pwm.c
  - adc.c
- libopencm3/
- cmake/
  - toolchain-arm-none-eabi.cmake
- pyocd-packs/
  - Puya.PY32.pack (pyOCD device pack)
- build/ (recommended build directory)

## Quick Start

### Build everything in 3 steps

```bash
# 1) Build libopencm3 (one-time)
cd libopencm3
make TARGETS=py32/f0
cd ..

# 2) Create and enter build directory
mkdir build
cd build

# 3) Configure and build all examples
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake ..
cmake --build . -j
```

This creates `blink.elf`, `uart.elf`, `pwm.elf`, `adc.elf`, and all matching
`.bin`/`.hex` files in `build/`.

### Flash firmware

From the `build/` directory:

```bash
# Flash and reset blink
cmake --build . --target flash_reset_blink -j

# Flash and reset uart
cmake --build . --target flash_reset_uart -j

# Flash and reset PWM or ADC
cmake --build . --target flash_reset_pwm -j
cmake --build . --target flash_reset_adc -j
```

## Example Selection

Use `APP` in CMake:

- `-DAPP=blink`
- `-DAPP=uart`
- `-DAPP=pwm`
- `-DAPP=adc`
- `-DAPP=all` (build all examples)

## Recommended Build Flow (pico-sdk style)

### 1) Build libopencm3 first

```bash
cd libopencm3
make TARGETS=py32/f0
```

### 2) Configure and build

#### Blink

```bash
cd ..
cmake -S . -B build/blink -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -DAPP=blink
cmake --build build/blink -j
```

Artifacts are generated in `build/blink/`:

- blink.elf
- blink.bin
- blink.hex

#### UART

```bash
cmake -S . -B build/uart -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -DAPP=uart
cmake --build build/uart -j
```

Artifacts are generated in `build/uart/`:

- uart.elf
- uart.bin
- uart.hex

#### Build all examples

```bash
cmake -S . -B build/all -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -DAPP=all
cmake --build build/all -j
```

## Flash Targets

### Single-app build (`APP=blink`, `uart`, `pwm`, or `adc`)

Global aliases are available:

- `flash`
- `flash-reset`

Example:

```bash
cmake --build build/uart --target flash-reset -j
```

### Multi-app build (`APP=all`)

Per-example targets are available:

- `flash_blink`
- `flash_uart`
- `flash_pwm`
- `flash_adc`
- `flash_reset_blink`
- `flash_reset_uart`
- `flash_reset_pwm`
- `flash_reset_adc`

Example:

```bash
cmake --build build/all --target flash_reset_uart -j
```

## pyOCD Settings

Default values:

- target: `py32f003x4`
- config: `build/pyocd.yaml` (auto-generated, not committed)

The `pyocd.yaml` file is automatically created in the `build/` directory on first `cmake`. Do not edit it manually.

Override via CMake:

```bash
cmake -S . -B build/uart \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=uart \
  -DPYOCD_TARGET=py32f003x4 \
  -DPYOCD_CONFIG=build/uart/pyocd.yaml
```

## UART Example Details

- UART: USART2
- TX: PA0 (AF9)
- RX: PA1 (AF9)
- Baudrate: 115200
- Behavior: echo incoming characters
- Includes startup stabilization to reduce garbled text after reset

## Troubleshooting

### `build/... is not a directory`

Configure first:

```bash
cmake -S . -B build/uart -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -DAPP=uart
```

### Garbled UART text at startup

- Verify terminal is 115200 8N1
- Verify common GND between board and USB-UART adapter
- Verify TX/RX are crossed correctly
- As a test, reduce baudrate to 57600
