# PY32F003 + libopencm3

Build and flash firmware for the PY32F003 with an ARM toolchain, CMake, and pyOCD. Select one example or build the full collection; each build produces `.elf`, `.bin`, and `.hex` files.

> [!TIP]
> Following the BugCON workshop? Start with the [workshop's four-step guide](https://github.com/UNIT-Electronics-MX/unit_py32_libopencm3): installation → compilation → PyOCD and probe → flashing. The instructions below also work when using this SDK on its own; run them from the `py32f003_lib/` directory.

| [Set up](docs/SETUP.md) | [Build](docs/BUILD.md) | [Flash](docs/FLASH.md) | [Hardware](docs/HARDWARE.md) |
| :---: | :---: | :---: | :---: |
| Toolchain and pyOCD | Build options and outputs | Program the board | Pins and wiring |

See the [documentation index](docs/INDEX.md) for the [examples](docs/EXAMPLES.md) and [troubleshooting guide](docs/TROUBLESHOOTING.md).

## Requirements

- `arm-none-eabi-gcc` and related ARM tools
- CMake 3.16 or newer
- `make` or `gmake`
- Python 3 and `pyocd` for flashing
- A compatible SWD probe and a PY32F003 board for hardware steps

## 1. Get the source

`libopencm3` is a Git submodule, so include it when cloning:

```bash
git clone --recurse-submodules https://github.com/Cesarbautista10/py32f003_lib.git
cd py32f003_lib
git submodule status --recursive
```

> [!IMPORTANT]
> GitHub's **Download ZIP** does not include submodules. If you already cloned without `--recurse-submodules`, run `git submodule update --init --recursive` here. A leading `-` in `git submodule status` means a submodule is missing.

## 2. Install the tools (Debian or Ubuntu)

```bash
sudo apt update
sudo apt install -y git cmake make gcc-arm-none-eabi binutils-arm-none-eabi \
  libnewlib-arm-none-eabi python3-venv libusb-1.0-0

python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install pyocd pyserial
```

Activate the environment again in each new terminal with `source .venv/bin/activate`. See [setup](docs/SETUP.md) for verification steps and other operating systems.

## 3. Build firmware

Run these commands from the **root of this SDK**:

```bash
# Build the library once.
make -C libopencm3 TARGETS=py32/f0

# Configure and build UART in its own directory.
cmake -S . -B build/uart \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DAPP=uart
cmake --build build/uart -j
```

Look for `build/uart/uart.elf`, `build/uart/uart.bin`, and `build/uart/uart.hex`. To build Blink, replace `uart` with `blink` in the configure and build commands.

> [!TIP]
> The CMake option is `--build`. `cmake --hash build/uart -j` is a typo and produces `Unknown argument`; `cmake --build build/uart -j` compiles the configured project.

### Choose an example

| `APP` value | Example |
| --- | --- |
| `blink` | LED blink |
| `uart` | USART2 echo on PA0 (TX) and PA1 (RX), 115200 baud, with startup stabilization |
| `reto_dump` | BugCON UART challenge for firmware extraction and disassembly practice |
| `pwm`, `pwm_pa2` | PWM demonstrations |
| `adc` | ADC demonstration |
| `i2c_pwm`, `i2c_on_off`, `i2c_adc` | I²C peripheral examples |
| `rgb_pa0` | I²C-controlled WS2812-compatible RGB LEDs on PA0 |
| `ws2812_pa0` | Standalone WS2812-compatible animation on PA0 |
| `all` | All examples (the default) |

For example, to build everything in a separate directory:

```bash
cmake -S . -B build/all -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -DAPP=all
cmake --build build/all -j
```

The Arduino master for `rgb_pa0` is in `examples/arduino_master/i2c_rgb/`. The workshop's discovery and control master is in `examples/arduino_master/i2c_network/`.

## 4. Flash a board

Connect the SWD probe and verify that pyOCD detects it:

```bash
pyocd list
```

For a **single-example build** such as `build/uart`, flash and reset with:

```bash
cmake --build build/uart --target flash-reset -j
```

For an **`APP=all` build**, select the example in the target name:

```bash
cmake --build build/all --target flash_reset_uart -j
```

Use `flash` or `flash_uart` to program without a reset. CMake creates `<build-directory>/pyocd.yaml` on configuration; it points to the local `pyocd-packs/Puya.PY32.pack` for the `py32f003x4` target. You can override the target and config path with `PYOCD_TARGET` and `PYOCD_CONFIG` when configuring CMake. See [flashing](docs/FLASH.md) for manual pyOCD commands.

## Project map

| Path | Purpose |
| --- | --- |
| `examples/` | Firmware examples and Arduino masters |
| `libopencm3/` | Adapted library, included as a Git submodule |
| `cmake/` | ARM toolchain configuration |
| `pyocd-packs/` | Local device pack for pyOCD |
| `docs/` | Setup, build, flash, hardware, and troubleshooting guides |
| `build/` | Generated output, created when you configure CMake |

## Need help?

- **`build/... is not a directory`:** configure that build directory with `cmake -S . -B build/<app> ... -DAPP=<app>` before using `cmake --build`.
- **Probe or device not detected:** check SWD wiring, power, the probe firmware, and `pyocd list`; see [hardware](docs/HARDWARE.md) and [troubleshooting](docs/TROUBLESHOOTING.md).
- **Garbled UART output:** use 115200 8N1, connect a common ground, and cross TX/RX. Try 57600 as a diagnostic test.
