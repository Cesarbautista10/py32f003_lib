# Makefile específico para PY32F003 v5 - I2C Migrado a PF0/PF1

##### Project #####
PROJECT = firmware_v5

# The path for generated files
BUILD_DIR = Build

# MCU type
MCU_TYPE = PY32F003x4

##### Options #####
USE_LL_LIB ?= n
ENABLE_PRINTF_FLOAT ?= n
USE_FREERTOS ?= n
USE_DSP ?= n
USE_EPAPER ?= n
FLASH_PROGRM ?= pyocd

##### Toolchains #######
ARM_TOOLCHAIN ?= /usr/bin
PYOCD_EXE ?= pyocd

##### Paths ############

# C source folders
SRC_DIRS = \
	.

# C sources
SRCS = \
	main.c \
	systemConfig.c \
	adc_control.c \
	pwm_control.c \
	neo_spi.c \
	py32f0xx_hal_msp.c \
	py32f0xx_it.c

# Include paths
INC_DIRS = \
	. \
	Libraries/CMSIS/Core/Include \
	Libraries/CMSIS/Device/PY32F0xx/Include \
	Libraries/PY32F0xx_HAL_Driver/Inc

##### Build Rules #####

# Default rule
all: $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).hex $(BUILD_DIR)/$(PROJECT).bin

# Include the main rules
include rules.mk

# Build specific targets
$(BUILD_DIR)/$(PROJECT).elf: $(OBJS)
	@echo "Linking: $@"
	@mkdir -p $(@D)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).elf
	@echo "Creating hex: $@"
	$(HEX) $< $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	@echo "Creating bin: $@"
	$(BIN) $< $@

# Clean
clean:
	-rm -fR $(BUILD_DIR)/$(PROJECT)*

# Flash the target
flash: $(BUILD_DIR)/$(PROJECT).hex
	$(PYOCD_EXE) flash -t $(MCU_TYPE) $<

.PHONY: all clean flash
