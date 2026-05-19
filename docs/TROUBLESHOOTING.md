# Troubleshooting

## Build Issues

### CMake: "Not a CMake build directory"

```
Error: not a CMake build directory (missing CMakeCache.txt)
```

**Solution**: Run cmake first

```bash
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake ..
cmake --build . -j
```

### CMake: "Unknown APP"

```
FATAL_ERROR Unknown APP='xyz'. Valid values: blink, uart, all
```

**Solution**: Use valid APP names

```bash
# Valid values:
cmake -DAPP=blink ..
cmake -DAPP=uart ..
cmake -DAPP=all ..
```

### Compiler: "arm-none-eabi-gcc: command not found"

**Solution**: Install ARM toolchain

```bash
# Ubuntu/Debian
sudo apt-get install -y arm-none-eabi-gcc arm-none-eabi-binutils

# macOS
brew install arm-none-eabi-gcc

# Verify
arm-none-eabi-gcc --version
```

### Build: "libopencm3_py32f0.a not found"

```
cannot find -lopencm3_py32f0
```

**Solution**: Build libopencm3 first

```bash
cd libopencm3
make TARGETS=py32/f0
cd ..
```

### Build: "linker.ld.S not found"

**Solution**: Ensure libopencm3 is a git submodule

```bash
git submodule update --init --recursive
cd libopencm3
make TARGETS=py32/f0
```

## Flash Issues

### pyOCD: "Device not found"

```
Error: [__main__] No connected devices found
```

**Diagnosis**:

```bash
# Check device detection
pyocd list

# Check USB connection
lsusb | grep -i "CMSIS\|ST-Link\|J-Link"
```

**Solution**:

1. **Verify USB cable**: Try different cable
2. **Check probe power**: LED on probe should be lit
3. **Restart probe**: Unplug and replug USB
4. **Check permissions**: 
   ```bash
   sudo pyocd list  # If works with sudo, permissions issue
   ```
5. **Install udev rules** (Linux):
   ```bash
   # For OpenDap/ST-Link
   sudo cp /usr/share/pyocd/udev/*.rules /etc/udev/rules.d/
   sudo udevadm control --reload
   ```

### pyOCD: "Configuration file not found"

```
Error: [__main__] 'Errno 2' No such file or directory: '../pyocd-packs/Puya.PY32.pack'
```

**Solution**: Ensure you run from `build/` directory

```bash
cd build  # Must be in build directory
cmake --build . --target flash_reset_uart -j
```

### pyOCD: "Target is not properly initialized"

```
Error: Target is not properly initialized
```

**Solution**: 
1. Verify SWD pins (SWCLK, SWDIO)
2. Check 3.3V power
3. Try reset:
   ```bash
   pyocd reset -t py32f003x4 --config build/pyocd.yaml
   ```

### Flash: "Timeout"

**Solution**: Add delay before flashing

```bash
sleep 2
cmake --build . --target flash_reset_uart -j
```

Or power-cycle the device and quickly run the command.

## UART Issues

### UART: "Garbled text at startup"

Example output:
```
PV&2�03 libopencm3 EC�
```

**Cause**: Oscillator/USB adapter not stabilized, RX line picking up noise

**Solution**: Already handled by `uart_link_stabilize()` function (2.4M cycle delay)

If still occurring:

1. **Verify connections**:
   - PA0 (TX) to adapter RX
   - PA1 (RX) to adapter TX
   - GND to adapter GND

2. **Increase stabilization delay** in `examples/uart.c`:
   ```c
   // Increase loop count from 2400000
   for (uint32_t i = 0; i < 5000000; i++);
   ```

3. **Use lower baud rate** temporarily:
   - Edit `examples/uart.c`
   - Change `usart_set_baudrate(USART2, 57600);`
   - Rebuild and flash

### UART: "No characters appear"

**Diagnosis**:

```bash
# Test with screen/minicom
screen /dev/ttyUSB0 115200

# If nothing appears, power cycle device while monitor running
```

**Solution**:

1. **Verify USB adapter is working**:
   ```bash
   cat /dev/ttyUSB0  # Should block waiting for data
   # Type from another terminal: echo "test" > /dev/ttyUSB0
   ```

2. **Check baud rate**:
   - Firmware: 115200
   - Terminal: 115200
   - Must match exactly

3. **Verify pin connections**:
   - PA0 → TX line (adapter RX)
   - PA1 → RX line (adapter TX)
   - GND → GND (adapter)

4. **Check USB adapter**: Try different adapter or known-good one

5. **Verify common GND**: Multiple GND connections required

### UART: "Partial or corrupted characters"

**Cause**: Baud rate mismatch, noise on lines, or poor connection

**Solution**:

1. **Double-check baud rate** (both code and terminal)
2. **Add current-limiting resistors** on TX/RX lines (1kΩ recommended)
3. **Shorten TX/RX wires** (reduce noise coupling)
4. **Check USB cable quality** (thick, shielded preferred)
5. **Test at lower baud rate**:
   ```c
   usart_set_baudrate(USART2, 57600);  // Instead of 115200
   ```

## Device Issues

### Device: "Won't program multiple times"

**Solution**: Add proper delays between flashes

```bash
cmake --build . --target flash_reset_uart -j
sleep 2  # Wait for device to restart
cmake --build . --target flash_reset_uart -j
```

### Device: "Program runs but doesn't respond"

**Possible causes**:

1. **Clock not configured**: Check `systemConfig.c` initialization
2. **Interrupts not enabled**: Verify NVIC setup
3. **GPIO not initialized**: Check pin mode configuration
4. **UART timing wrong**: Verify clock tree setup

**Debug approach**:

1. Test with blink example (simpler)
2. Add LED toggling to UART example to indicate execution
3. Check system clock frequency

## Getting Help

### Information to provide when asking for help:

1. **Device**: PY32F003X4 / other
2. **OS**: Ubuntu 22.04 / macOS / WSL / other
3. **Error message**: Complete output
4. **Build command**: Exact cmake/make commands used
5. **Hardware setup**: Pin connections, adapter type
6. **Steps to reproduce**: Exact sequence to trigger issue

### Useful debug commands:

```bash
# Check toolchain
arm-none-eabi-gcc --version

# Check cmake
cmake --version

# Check pyocd
pyocd --version

# List connected devices
pyocd list

# Run verbose flash (more details)
pyocd flash -t py32f003x4 --config build/pyocd.yaml -vvv build/uart.hex
```

## Next Steps

- [Review hardware setup](HARDWARE.md)
- [Check example details](EXAMPLES.md)
- [Verify build process](BUILD.md)
