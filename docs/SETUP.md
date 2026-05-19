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
sudo apt-get install -y arm-none-eabi-gcc arm-none-eabi-binutils cmake make
```

#### macOS
```bash
brew install arm-none-eabi-gcc cmake
```

#### Windows (WSL)
```bash
sudo apt-get update
sudo apt-get install -y arm-none-eabi-gcc cmake make
```

### 2. pyOCD (for flashing)

```bash
pip install pyocd
```

Verify installation:
```bash
pyocd --version
```

### 3. Clone Repository

```bash
git clone <repository-url>
cd unit_py32_locknode_firmware
git submodule update --init --recursive
```

### 4. Verify Setup

Check all dependencies:
```bash
cmake --version
arm-none-eabi-gcc --version
make --version
pyocd --version
```

All commands should print version information without errors.

## Next Steps

- [Build the firmware](BUILD.md)
- [Flash to device](FLASH.md)
