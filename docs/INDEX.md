# Documentation Index

Welcome to the PY32F003 + libopencm3 SDK documentation. Choose a topic:

## Getting Started

- **[Setup](SETUP.md)** - Install dependencies and prerequisites
- **[Build](BUILD.md)** - Compile firmware with CMake

## Development

- **[Flashing](FLASH.md)** - Program firmware to device
- **[Examples](EXAMPLES.md)** - Available examples and their details
- **[Hardware](HARDWARE.md)** - Hardware connections and pinouts

## Troubleshooting

- **[Troubleshooting](TROUBLESHOOTING.md)** - Common issues and solutions

## Project Structure

```
.
├── examples/           # Source code for blink, uart, pwm, and adc examples
├── libopencm3/        # ARM Cortex-M0+ library (git submodule)
├── cmake/             # CMake toolchain for ARM cross-compilation
├── pyocd-packs/       # pyOCD device packs
├── build/             # Build output directory (created during build)
└── docs/              # This documentation
```

## Quick Commands

Build all examples:
```bash
cd libopencm3 && make TARGETS=py32/f0 && cd ..
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake ..
cmake --build . -j
```

Flash uart example:
```bash
cmake --build . --target flash_reset_uart -j
```
