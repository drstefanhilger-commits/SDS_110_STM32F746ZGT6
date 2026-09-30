# SDS_110_STM32F746ZGT6 – Kommandozeilen-Build unter WSL2/Linux
#
# Bildet die Debug-Konfiguration aus .cproject (STM32CubeIDE 2.2.0) nach.
# Toolchain: Arm GNU Toolchain 14.3.rel1 (wie CubeIDE), Standard unter ~/opt.
#
#   make -C wsl -j            # Debug-Build -> build/wsl/SDS_110_STM32F746ZGT6.elf/.bin/.hex/.map
#   make -C wsl -j OPT=-O2    # andere Optimierung
#   make -C wsl clean
#   make -C wsl TOOLCHAIN=/pfad/zur/toolchain
#   make -C wsl PREFIX=arm-none-eabi-   # Toolchain aus dem PATH (z. B. Paket gcc-arm-none-eabi)
#
# Neue Quellen unter Core, Drivers, Middlewares, USB_DEVICE werden automatisch
# gefunden (wie die Quellordner in CubeIDE).

PROJ      := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
BUILD     ?= $(PROJ)/build/wsl
TARGET    := SDS_110_STM32F746ZGT6
TOOLCHAIN ?= $(HOME)/opt/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi
PREFIX    ?= $(TOOLCHAIN)/bin/arm-none-eabi-
CC      := $(PREFIX)gcc
CXX     := $(PREFIX)g++
SIZE    := $(PREFIX)size
OBJCOPY := $(PREFIX)objcopy

SRCDIRS  := Core Drivers Middlewares USB_DEVICE
FIND      = $(shell cd $(PROJ) && find $(SRCDIRS) -name '$(1)')
C_SRCS   := $(call FIND,*.c)
CPP_SRCS := $(call FIND,*.cpp)
S_SRCS   := $(call FIND,*.s)

INCDIRS := Core/Inc Drivers/STM32F7xx_HAL_Driver/Inc \
  Drivers/STM32F7xx_HAL_Driver/Inc/Legacy Middlewares/Third_Party/FreeRTOS/Source/include \
  Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 \
  Middlewares/ST/ARM/DSP/Inc \
  Drivers/CMSIS/Device/ST/STM32F7xx/Include Drivers/CMSIS/Include Drivers/CMSIS \
  USB_DEVICE/App USB_DEVICE/Target Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
  Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc \
  $(shell cd $(PROJ) && find Core/SDS_110 -type d)
INCS := $(addprefix -I$(PROJ)/,$(INCDIRS))

MCU      := -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb
DEFS     := -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx
OPT      ?= -O0 -g3
LDSCRIPT ?= $(PROJ)/STM32F746ZGTX_FLASH.ld
COMMON   := $(MCU) $(DEFS) $(INCS) $(OPT) -ffunction-sections -fdata-sections -Wall \
            -fstack-usage --specs=nano.specs -MMD -MP
CFLAGS   := $(COMMON) -std=gnu11
CXXFLAGS := $(COMMON) -std=gnu++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit
ASFLAGS  := $(MCU) -g3 -DDEBUG -x assembler-with-cpp --specs=nano.specs
LDFLAGS  := $(MCU) -T$(LDSCRIPT) --specs=nosys.specs --specs=nano.specs -u _printf_float \
            -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--gc-sections -static -Wl,--no-warn-rwx-segments \
            -L$(PROJ)/Drivers/CMSIS/Lib/GCC -Wl,--print-memory-usage
LIBS     := -Wl,--start-group -larm_cortexM7lfsp_math -lc -lm -lstdc++ -lsupc++ -Wl,--end-group

OBJS := $(addprefix $(BUILD)/,$(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o) $(S_SRCS:.s=.o))
ELF  := $(BUILD)/$(TARGET).elf

all: $(ELF) $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex

$(BUILD)/%.o: $(PROJ)/%.c
	@mkdir -p $(dir $@)
	@echo "CC   $*.c"
	@$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: $(PROJ)/%.cpp
	@mkdir -p $(dir $@)
	@echo "CXX  $*.cpp"
	@$(CXX) $(CXXFLAGS) -c $< -o $@
$(BUILD)/%.o: $(PROJ)/%.s
	@mkdir -p $(dir $@)
	@echo "AS   $*.s"
	@$(CC) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD   $(TARGET).elf"
	@$(CXX) $(OBJS) $(LDFLAGS) $(LIBS) -o $@
	@$(SIZE) $@
$(BUILD)/$(TARGET).bin: $(ELF)
	@$(OBJCOPY) -O binary $< $@
$(BUILD)/$(TARGET).hex: $(ELF)
	@$(OBJCOPY) -O ihex $< $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
-include $(OBJS:.o=.d)
