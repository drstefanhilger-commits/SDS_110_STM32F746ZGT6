################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/freertos.c \
../Core/Src/main.c \
../Core/Src/stm32f7xx_hal_msp.c \
../Core/Src/stm32f7xx_hal_timebase_tim.c \
../Core/Src/stm32f7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f7xx.c 

C_DEPS += \
./Core/Src/freertos.d \
./Core/Src/main.d \
./Core/Src/stm32f7xx_hal_msp.d \
./Core/Src/stm32f7xx_hal_timebase_tim.d \
./Core/Src/stm32f7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f7xx.d 

OBJS += \
./Core/Src/freertos.o \
./Core/Src/main.o \
./Core/Src/stm32f7xx_hal_msp.o \
./Core/Src/stm32f7xx_hal_timebase_tim.o \
./Core/Src/stm32f7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f7xx.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/ST/ARM/DSP/Inc -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Data_Interface_140" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Harness" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Sensor_Unit_112" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Driver" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Model" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Tasks" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Timer" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Utils" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Correlation_Processing_Module_126" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Feature_Extraction_Module_122" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Localisation_Module_128" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Machine_Learning_Module_124" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Output_Interface_130" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/freertos.cyclo ./Core/Src/freertos.d ./Core/Src/freertos.o ./Core/Src/freertos.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/stm32f7xx_hal_msp.cyclo ./Core/Src/stm32f7xx_hal_msp.d ./Core/Src/stm32f7xx_hal_msp.o ./Core/Src/stm32f7xx_hal_msp.su ./Core/Src/stm32f7xx_hal_timebase_tim.cyclo ./Core/Src/stm32f7xx_hal_timebase_tim.d ./Core/Src/stm32f7xx_hal_timebase_tim.o ./Core/Src/stm32f7xx_hal_timebase_tim.su ./Core/Src/stm32f7xx_it.cyclo ./Core/Src/stm32f7xx_it.d ./Core/Src/stm32f7xx_it.o ./Core/Src/stm32f7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f7xx.cyclo ./Core/Src/system_stm32f7xx.d ./Core/Src/system_stm32f7xx.o ./Core/Src/system_stm32f7xx.su

.PHONY: clean-Core-2f-Src

