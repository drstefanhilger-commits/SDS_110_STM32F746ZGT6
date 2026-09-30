################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/SDS_110/Processing_Module_120/Processing_Module_120.cpp 

OBJS += \
./Core/SDS_110/Processing_Module_120/Processing_Module_120.o 

CPP_DEPS += \
./Core/SDS_110/Processing_Module_120/Processing_Module_120.d 


# Each subdirectory must supply rules for building sources it contributes
Core/SDS_110/Processing_Module_120/%.o Core/SDS_110/Processing_Module_120/%.su Core/SDS_110/Processing_Module_120/%.cyclo: ../Core/SDS_110/Processing_Module_120/%.cpp Core/SDS_110/Processing_Module_120/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/ST/ARM/DSP/Inc -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Data_Interface_140" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Harness" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Sensor_Unit_112" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Driver" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Model" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Tasks" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Timer" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Utils" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Correlation_Processing_Module_126" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Feature_Extraction_Module_122" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Localisation_Module_128" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Machine_Learning_Module_124" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Output_Interface_130" -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-SDS_110-2f-Processing_Module_120

clean-Core-2f-SDS_110-2f-Processing_Module_120:
	-$(RM) ./Core/SDS_110/Processing_Module_120/Processing_Module_120.cyclo ./Core/SDS_110/Processing_Module_120/Processing_Module_120.d ./Core/SDS_110/Processing_Module_120/Processing_Module_120.o ./Core/SDS_110/Processing_Module_120/Processing_Module_120.su

.PHONY: clean-Core-2f-SDS_110-2f-Processing_Module_120

