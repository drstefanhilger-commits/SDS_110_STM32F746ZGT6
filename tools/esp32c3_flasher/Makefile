# ESP32-C3-Flasher für das SDS_110-Board – eigene kleine STM32-Firmware (README.md)
#
#   make                                   # Images aus ../esp32c3_bridge/build einbetten, bauen
#   make ESP_BUILD=/pfad/zum/esp/build     # anderer Build der Brücke
#   make TOOLCHAIN=~/opt/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi
#   make test                              # Protokoll gegen simuliertes ROM (Host, g++)
#   make clean
#
# Ergebnis: build/esp32c3_flasher.elf/.bin/.hex – mit ST-LINK aufspielen (ersetzt die SDS-Firmware).

ROOT      := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
HERE      := $(ROOT)/tools/esp32c3_flasher
ESP_BUILD ?= $(ROOT)/tools/esp32c3_bridge/build
BUILD     ?= $(HERE)/build
TARGET    := esp32c3_flasher
TOOLCHAIN ?=
PREFIX    ?= $(if $(TOOLCHAIN),$(TOOLCHAIN)/bin/,)arm-none-eabi-
CC      := $(PREFIX)gcc
CXX     := $(PREFIX)g++
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size
PYTHON  ?= python3

HAL := $(ROOT)/Drivers/STM32F7xx_HAL_Driver/Src
C_SRCS := $(ROOT)/Core/Src/system_stm32f7xx.c $(ROOT)/Core/Src/syscalls.c $(ROOT)/Core/Src/sysmem.c \
  $(addprefix $(HAL)/stm32f7xx_hal,.c _cortex.c _rcc.c _rcc_ex.c _gpio.c _uart.c _uart_ex.c _dma.c _pwr.c _pwr_ex.c)
CPP_SRCS := $(HERE)/src/main.cpp
S_SRCS   := $(ROOT)/Core/Startup/startup_stm32f746zgtx.s

INCS := -I$(HERE)/src -I$(ROOT)/Core/Inc -I$(ROOT)/Drivers/STM32F7xx_HAL_Driver/Inc \
  -I$(ROOT)/Drivers/CMSIS/Device/ST/STM32F7xx/Include -I$(ROOT)/Drivers/CMSIS/Include

MCU      := -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb
COMMON   := $(MCU) -DUSE_HAL_DRIVER -DSTM32F746xx $(INCS) -Os -g3 -ffunction-sections -fdata-sections \
            -Wall --specs=nano.specs -MMD -MP
CFLAGS   := $(COMMON) -std=gnu11
CXXFLAGS := $(COMMON) -std=gnu++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
ASFLAGS  := $(MCU) -g3 -x assembler-with-cpp
LDSCRIPT := $(ROOT)/STM32F746ZGTX_FLASH.ld
LDFLAGS  := $(MCU) -T$(LDSCRIPT) --specs=nosys.specs --specs=nano.specs -Wl,-Map=$(BUILD)/$(TARGET).map \
            -Wl,--gc-sections -static -Wl,--no-warn-rwx-segments -Wl,--print-memory-usage

obj = $(BUILD)/obj/$(notdir $(basename $(1))).o
OBJS := $(foreach f,$(C_SRCS) $(CPP_SRCS) $(S_SRCS),$(call obj,$(f))) $(BUILD)/obj/esp_images.o
ELF  := $(BUILD)/$(TARGET).elf

all: $(ELF) $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex

$(BUILD)/esp_images.S: $(ESP_BUILD)/flash_args $(HERE)/mkimages.py $(wildcard $(ESP_BUILD)/*.bin $(ESP_BUILD)/*/*.bin)
	$(PYTHON) $(HERE)/mkimages.py $(ESP_BUILD) $@

$(BUILD)/obj/esp_images.o: $(BUILD)/esp_images.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

define rule
$(call obj,$(1)): $(1)
	@mkdir -p $$(dir $$@)
	$(2) -c $$< -o $$@
endef
$(foreach f,$(C_SRCS),$(eval $(call rule,$(f),$$(CC) $$(CFLAGS))))
$(foreach f,$(CPP_SRCS),$(eval $(call rule,$(f),$$(CXX) $$(CXXFLAGS))))
$(foreach f,$(S_SRCS),$(eval $(call rule,$(f),$$(CC) $$(ASFLAGS))))

$(ELF): $(OBJS) $(LDSCRIPT)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).bin: $(ELF)
	$(OBJCOPY) -O binary $< $@
$(BUILD)/$(TARGET).hex: $(ELF)
	$(OBJCOPY) -O ihex $< $@

test: $(BUILD)/test_loader
	$(BUILD)/test_loader
$(BUILD)/test_loader: $(HERE)/test/test_loader.cpp $(HERE)/src/EspRomLoader.hpp
	@mkdir -p $(dir $@)
	g++ -std=gnu++17 -O2 -Wall -Wextra -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
-include $(OBJS:.o=.d)
