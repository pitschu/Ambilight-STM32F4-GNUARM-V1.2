################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include usb_vcp/usb_cdc_lolevel/subdir.mk
-include usb_vcp/subdir.mk
-include STM32F4xx_StdPeriph_Driver/src/subdir.mk
-include CMSIS/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH += \
Ambilight-STM32F4-GNUARM-V1.1.hex \

SECONDARY_LIST += \
Ambilight-STM32F4-GNUARM-V1.1.lst \

SECONDARY_SIZE += \
Ambilight-STM32F4-GNUARM-V1.1.siz \


# All Target
all: Ambilight-STM32F4-GNUARM-V1.1.elf secondary-outputs

# Tool invocations
Ambilight-STM32F4-GNUARM-V1.1.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: ARM Summon Windows GCC C Linker'
	arm-none-eabi-gcc -T"../stm32f407_flash.ld" -nostartfiles -Wl,-Map,Ambilight-STM32F4-GNUARM-V1.1.map -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -g3 -gdwarf-2 -o "Ambilight-STM32F4-GNUARM-V1.1.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

Ambilight-STM32F4-GNUARM-V1.1.hex: Ambilight-STM32F4-GNUARM-V1.1.elf
	@echo 'Invoking: ARM Summon Windows GNU Create Flash Image'
	arm-none-eabi-objcopy -O ihex Ambilight-STM32F4-GNUARM-V1.1.elf  "Ambilight-STM32F4-GNUARM-V1.1.hex"
	@echo 'Finished building: $@'
	@echo ' '

Ambilight-STM32F4-GNUARM-V1.1.lst: Ambilight-STM32F4-GNUARM-V1.1.elf
	@echo 'Invoking: ARM Summon Windows GNU Create Listing'
	arm-none-eabi-objdump -h -S Ambilight-STM32F4-GNUARM-V1.1.elf > "Ambilight-STM32F4-GNUARM-V1.1.lst"
	@echo 'Finished building: $@'
	@echo ' '

Ambilight-STM32F4-GNUARM-V1.1.siz: Ambilight-STM32F4-GNUARM-V1.1.elf
	@echo 'Invoking: ARM Summon Windows GNU Print Size'
	arm-none-eabi-size  --format=berkeley Ambilight-STM32F4-GNUARM-V1.1.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(SECONDARY_SIZE)$(OBJS)$(C_DEPS)$(ASM_DEPS)$(SECONDARY_FLASH)$(EXECUTABLES)$(SECONDARY_LIST)$(S_UPPER_DEPS) Ambilight-STM32F4-GNUARM-V1.1.elf
	-@echo ' '

secondary-outputs: $(SECONDARY_FLASH) $(SECONDARY_LIST) $(SECONDARY_SIZE)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
