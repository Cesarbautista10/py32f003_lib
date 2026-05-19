# Build - Compiling Firmware

## Prerequisites

Complete [Setup](SETUP.md) first.

## Build libopencm3

This only needs to be done once:

```bash
cd libopencm3
make TARGETS=py32/f0
cd ..
```

This compiles `libopencm3_py32f0.a` which all examples depend on.

## Build All Examples

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake ..
cmake --build . -j
```

This creates:
- `build/blink.elf`
- `build/uart.elf`
- `build/blink.hex` / `build/uart.hex`
- `build/blink.bin` / `build/uart.bin`

## Build Single Example

### Blink Only

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=blink ..
cmake --build . -j
```

### UART Only

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=uart ..
cmake --build . -j
```

## Build Configuration

### APP Variable

Control which examples to build:

- `-DAPP=blink` - Build blink example only
- `-DAPP=uart` - Build uart example only
- `-DAPP=all` - Build all examples (default)

### CMake Invocation Examples

```bash
# Blink only
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=blink ..

# UART only
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=uart ..

# All examples
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=all ..

# Or just use defaults (APP=all)
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake ..
```

## Build Output

After successful compilation:

```
build/
├── blink.elf              # Executable (multi-app builds)
├── blink.hex              # Hex format for flashing
├── blink.bin              # Binary format
├── blink.map              # Linker map file
├── uart.elf               # Executable (multi-app builds)
├── uart.hex               # Hex format for flashing
├── uart.bin               # Binary format
├── uart.map               # Linker map file
├── generated.py32f003x4.ld  # Generated linker script
└── pyocd.yaml             # Auto-generated pyOCD config
```

## Rebuild

To rebuild after code changes:

```bash
cd build
cmake --build . -j
```

To clean all build artifacts:

```bash
cd ..
rm -rf build
```

## Next Steps

- [Flash firmware to device](FLASH.md)
- [Learn about examples](EXAMPLES.md)
- [View hardware setup](HARDWARE.md)
