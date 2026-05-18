##
## Blink example for PY32F003 using libopencm3.
##

BINARY = blink

OPENCM3_DIR = ../libopencm3
DEVICE      = py32f003x4

# Optional cross-prefix override (note: hyphen is appended by gcc-config.mk)
PREFIX     ?= arm-none-eabi

OBJS       += main.o

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

# Use libopencm3's reset_handler as entry; skip newlib startup code and
# avoid pulling in newlib system calls.
LDFLAGS    += -nostartfiles --specs=nano.specs --specs=nosys.specs \
              -Wl,--gc-sections -Wl,-Map=$(BINARY).map
CFLAGS     += -Os -ggdb3 -Wall -Wextra -Wundef -Wshadow \
              -fno-common -ffunction-sections -fdata-sections

.PHONY: all clean flash

all: $(BINARY).elf $(BINARY).bin $(BINARY).hex

clean:
	$(Q)$(RM) -rf generated.*.ld *.o *.d *.elf *.bin *.hex *.map *.list *.lst

flash: 
	$(Q)pyocd flash -t py32f003x4 --config ./Misc/pyocd.yaml ./$(BINARY).hex

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk
