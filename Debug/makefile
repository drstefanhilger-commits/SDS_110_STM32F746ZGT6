################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include USB_DEVICE/Target/subdir.mk
-include USB_DEVICE/App/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/subdir.mk
-include Middlewares/ST/STM32_USB_Device_Library/Core/Src/subdir.mk
-include Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/subdir.mk
-include Drivers/STM32F7xx_HAL_Driver/Src/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include Core/SDS_110/Sensor_Unit_112/subdir.mk
-include Core/SDS_110/Processing_Module_120/Output_Interface_130/subdir.mk
-include Core/SDS_110/Processing_Module_120/Machine_Learning_Module_124/subdir.mk
-include Core/SDS_110/Processing_Module_120/Localisation_Module_128/subdir.mk
-include Core/SDS_110/Processing_Module_120/Feature_Extraction_Module_122/subdir.mk
-include Core/SDS_110/Processing_Module_120/Correlation_Processing_Module_126/subdir.mk
-include Core/SDS_110/Processing_Module_120/subdir.mk
-include Core/SDS_110/Infrastructure/Utils/subdir.mk
-include Core/SDS_110/Infrastructure/Timer/subdir.mk
-include Core/SDS_110/Infrastructure/Tasks/subdir.mk
-include Core/SDS_110/Infrastructure/Model/subdir.mk
-include Core/SDS_110/Infrastructure/Driver/subdir.mk
-include Core/SDS_110/Harness/subdir.mk
-include Core/SDS_110/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(CCM_DEPS)),)
-include $(CCM_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CXXM_DEPS)),)
-include $(CXXM_DEPS)
endif
ifneq ($(strip $(C++M_DEPS)),)
-include $(C++M_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := SDS_110_STM32F746ZGT6
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
SDS_110_STM32F746ZGT6.elf \

MAP_FILES += \
SDS_110_STM32F746ZGT6.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
SDS_110_STM32F746ZGT6.list \


# All Target
all: main-build

# Main-build Target
main-build: SDS_110_STM32F746ZGT6.elf secondary-outputs

# Tool invocations
SDS_110_STM32F746ZGT6.elf SDS_110_STM32F746ZGT6.map: $(OBJS) $(USER_OBJS) C:\Users\310004\Documents\Projects\Sound_Detection\project\SDS_110_STM32F746ZGT6\STM32F746ZGTX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-g++ -o "SDS_110_STM32F746ZGT6.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m7 -T"C:\Users\310004\Documents\Projects\Sound_Detection\project\SDS_110_STM32F746ZGT6\STM32F746ZGTX_FLASH.ld" --specs=nosys.specs -Wl,-Map="SDS_110_STM32F746ZGT6.map" -Wl,--gc-sections -static -L"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Drivers/CMSIS/Lib/GCC" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -u _printf_float -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

SDS_110_STM32F746ZGT6.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "SDS_110_STM32F746ZGT6.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) SDS_110_STM32F746ZGT6.elf SDS_110_STM32F746ZGT6.list SDS_110_STM32F746ZGT6.map default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
