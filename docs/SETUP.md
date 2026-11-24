# Setup - Installation and Prerequisites

## System Requirements

- **OS**: Linux, macOS, or WSL on Windows
- **CMake**: 3.16 or newer
- **ARM Toolchain**: arm-none-eabi-gcc
- **Build tools**: make or gmake
- **Python**: 3.8+ (for pyocd)

## Installation

### 1. ARM Toolchain

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi cmake make git python3-venv
```

#### macOS
```bash
brew install arm-none-eabi-gcc cmake
```

#### Windows (WSL)
```bash
sudo apt-get update
sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi cmake make git python3-venv
```

### 2. pyOCD (for flashing)

Install pyOCD in a virtual environment. Debian 12+ and recent Ubuntu reject
system-wide `pip install` (PEP 668 "externally-managed-environment"):

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install pyocd pyserial
```

Run `source .venv/bin/activate` again in every new terminal.

Verify installation:
```bash
pyocd --version
```

### 3. Clone Repository (with submodules)

`libopencm3` is a Git submodule and must be fetched together with the project:

```bash
git clone --recurse-submodules https://github.com/Cesarbautista10/py32f003_lib.git
cd py32f003_lib
git submodule status --recursive
```

If you cloned without `--recurse-submodules`, the `libopencm3/` folder is empty. Fix it with:

```bash
git submodule update --init --recursive
```

GitHub's "Download ZIP" does not include submodules; clone with Git instead.

### 4. Verify Setup

Check all dependencies:
```bash
cmake --version
arm-none-eabi-gcc --version
make --version
pyocd --version
ls libopencm3/Makefile
```

All commands should print version information without errors.

## Next Steps

- [Build the firmware](BUILD.md)
- [Flash to device](FLASH.md)
