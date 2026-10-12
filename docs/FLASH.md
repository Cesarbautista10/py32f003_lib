# Flash - Programming Firmware

## Prerequisites

- Compiled firmware (see [Build](BUILD.md))
- pyOCD installed
- Device connected via debug probe (OpenDap, J-Link, etc.)

## Verify Device Detection

```bash
pyocd list
```

Should show your PY32F003 device and debug probe.

## Flash From Build Directory

Navigate to the `build/` directory and use CMake targets:

### Multi-app Build (APP=all)

Flash specific examples:

```bash
# Flash and reset blink
cmake --build . --target flash_reset_blink -j

# Flash and reset uart
cmake --build . --target flash_reset_uart -j

# Flash and reset PWM, ADC, or the standalone WS2812 demo
cmake --build . --target flash_reset_pwm -j
cmake --build . --target flash_reset_adc -j
cmake --build . --target flash_reset_ws2812_pa0 -j

# Flash without reset
cmake --build . --target flash_blink -j
cmake --build . --target flash_uart -j
cmake --build . --target flash_pwm -j
cmake --build . --target flash_adc -j
```

### Single-app Build (APP=blink, uart, pwm, or adc)

Global aliases available:

```bash
# Flash and reset
cmake --build . --target flash-reset -j

# Flash without reset
cmake --build . --target flash -j
```

## Manual Flash with pyOCD

If CMake targets don't work:

```bash
cd build

# Flash hex file
pyocd flash -t py32f003x4 --config pyocd.yaml uart.hex

# Flash and reset
pyocd flash -t py32f003x4 --config pyocd.yaml uart.hex
pyocd reset -t py32f003x4 --config pyocd.yaml
```

## Verify Flash

After flashing, verify the device:

```bash
pyocd reset -t py32f003x4 --config pyocd.yaml
```

For UART example, open serial terminal at 115200 8N1 and verify output.

## Troubleshooting

### "Device not found"

1. Check device is powered
2. Check debug probe connection
3. Verify USB connection
4. Run `pyocd list` to see detected devices

### "Configuration file not found"

Ensure you run from `build/` directory where `pyocd.yaml` exists:

```bash
cd build
cmake --build . --target flash_reset_uart -j
```

### Flash Timeouts

Some devices need warm-up time:

```bash
# Add delay and retry
sleep 2
cmake --build . --target flash_reset_uart -j
```

## Examples

### Quick Flash and Test UART

```bash
cd build
cmake --build . --target flash_reset_uart -j

# In another terminal, open serial monitor
screen /dev/ttyUSB0 115200
# or
minicom -b 115200 -o -D /dev/ttyUSB0
```

### Flash All Examples

```bash
cd build
cmake --build . --target flash_reset_blink -j
# Verify blink LED ...

cmake --build . --target flash_reset_uart -j
# Verify UART output ...
```

## Next Steps

- [Test the UART example](EXAMPLES.md#uart-example)
- [Troubleshoot issues](TROUBLESHOOTING.md)
- [View hardware setup](HARDWARE.md)
