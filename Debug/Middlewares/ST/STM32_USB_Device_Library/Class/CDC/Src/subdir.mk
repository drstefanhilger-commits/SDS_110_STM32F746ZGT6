################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c 

C_DEPS += \
./Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.d 

OBJS += \
./Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.o 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/%.o Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/%.su Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/%.cyclo: ../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/%.c Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/ST/ARM/DSP/Inc -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Data_Interface_140" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Harness" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Sensor_Unit_112" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Driver" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Model" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Tasks" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Timer" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Infrastructure/Utils" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Correlation_Processing_Module_126" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Feature_Extraction_Module_122" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Localisation_Module_128" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Machine_Learning_Module_124" -I"C:/Users/310004/Documents/Projects/Sound_Detection/project/SDS_110_STM32F746ZGT6/Core/SDS_110/Processing_Module_120/Output_Interface_130" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-ST-2f-STM32_USB_Device_Library-2f-Class-2f-CDC-2f-Src

clean-Middlewares-2f-ST-2f-STM32_USB_Device_Library-2f-Class-2f-CDC-2f-Src:
	-$(RM) ./Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.cyclo ./Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.d ./Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.o ./Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.su

.PHONY: clean-Middlewares-2f-ST-2f-STM32_USB_Device_Library-2f-Class-2f-CDC-2f-Src

