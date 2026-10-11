################################################################################
# Automatically-generated file. Do not edit!
################################################################################

CG_TOOL_ROOT := /Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-msp430_21.6.3.LTS

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"./main.obj" \
"./GrLib/fonts/fontcm12.obj" \
"./GrLib/fonts/fontcm12b.obj" \
"./GrLib/fonts/fontcm16.obj" \
"./GrLib/fonts/fontcmss12.obj" \
"./GrLib/fonts/fontcmtt12.obj" \
"./GrLib/fonts/fontfixed6x8.obj" \
"./GrLib/grlib/button.obj" \
"./GrLib/grlib/checkbox.obj" \
"./GrLib/grlib/circle.obj" \
"./GrLib/grlib/context.obj" \
"./GrLib/grlib/display.obj" \
"./GrLib/grlib/image.obj" \
"./GrLib/grlib/imageButton.obj" \
"./GrLib/grlib/line.obj" \
"./GrLib/grlib/radioButton.obj" \
"./GrLib/grlib/rectangle.obj" \
"./GrLib/grlib/string.obj" \
"./LcdDriver/lcd_driver.obj" \
"./LcdDriver/middle_driver.obj" \
"../lnk_msp430fr6989.cmd" \
$(GEN_CMDS__FLAG) \
-llibmpu_init.a \
-llibmath.a \
-llibc.a \

-include ../makefile.init

RM := rm -rf
RMDIR := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include GrLib/fonts/subdir_vars.mk
-include GrLib/grlib/subdir_vars.mk
-include LcdDriver/subdir_vars.mk
-include subdir_rules.mk
-include GrLib/fonts/subdir_rules.mk
-include GrLib/grlib/subdir_rules.mk
-include LcdDriver/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
OkLastTimeBeforeIGiveUp.out 

EXE_OUTPUTS__QUOTED += \
"OkLastTimeBeforeIGiveUp.out" 


# All Target
all: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "OkLastTimeBeforeIGiveUp.out"

# Tool invocations
OkLastTimeBeforeIGiveUp.out: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@echo 'MSP430 Linker - building target: "$@"'
	"/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-msp430_21.6.3.LTS/bin/cl430" -vmspx --data_model=restricted --use_hw_mpy=F5 --advice:power=all --advice:hw_config=all --define=__MSP430FR6989__ --define=_MPU_ENABLE -g --printf_support=minimal --diag_warning=225 --diag_wrap=off --display_error_number --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU40 -z -m"OkLastTimeBeforeIGiveUp.map" --heap_size=160 --stack_size=160 --cinit_hold_wdt=on -i"/Applications/ti/ccs2101/ccs/ccs_base/msp430/include" -i"/Applications/ti/ccs2101/ccs/ccs_base/msp430/lib/5xx_6xx_FRxx" -i"/Applications/ti/ccs2101/ccs/ccs_base/msp430/lib/FR59xx" -i"/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-msp430_21.6.3.LTS/lib" -i"/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-msp430_21.6.3.LTS/include" --priority --reread_libs --define=_MPU_ENABLE --diag_wrap=off --display_error_number --warn_sections --xml_link_info="OkLastTimeBeforeIGiveUp_linkInfo.xml" --use_hw_mpy=F5 --rom_model -o "OkLastTimeBeforeIGiveUp.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXE_OUTPUTS__QUOTED)
	-$(RM) "main.obj" "GrLib/fonts/fontcm12.obj" "GrLib/fonts/fontcm12b.obj" "GrLib/fonts/fontcm16.obj" "GrLib/fonts/fontcmss12.obj" "GrLib/fonts/fontcmtt12.obj" "GrLib/fonts/fontfixed6x8.obj" "GrLib/grlib/button.obj" "GrLib/grlib/checkbox.obj" "GrLib/grlib/circle.obj" "GrLib/grlib/context.obj" "GrLib/grlib/display.obj" "GrLib/grlib/image.obj" "GrLib/grlib/imageButton.obj" "GrLib/grlib/line.obj" "GrLib/grlib/radioButton.obj" "GrLib/grlib/rectangle.obj" "GrLib/grlib/string.obj" "LcdDriver/lcd_driver.obj" "LcdDriver/middle_driver.obj" 
	-$(RM) "main.d" "GrLib/fonts/fontcm12.d" "GrLib/fonts/fontcm12b.d" "GrLib/fonts/fontcm16.d" "GrLib/fonts/fontcmss12.d" "GrLib/fonts/fontcmtt12.d" "GrLib/fonts/fontfixed6x8.d" "GrLib/grlib/button.d" "GrLib/grlib/checkbox.d" "GrLib/grlib/circle.d" "GrLib/grlib/context.d" "GrLib/grlib/display.d" "GrLib/grlib/image.d" "GrLib/grlib/imageButton.d" "GrLib/grlib/line.d" "GrLib/grlib/radioButton.d" "GrLib/grlib/rectangle.d" "GrLib/grlib/string.d" "LcdDriver/lcd_driver.d" "LcdDriver/middle_driver.d" 
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

