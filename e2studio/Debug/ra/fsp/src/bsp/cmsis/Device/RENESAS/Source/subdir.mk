################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c \
../ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c 

C_DEPS += \
./ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.d \
./ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.d 

CREF += \
mipi_csi_ek_ra8p1_ep.cref 

OBJS += \
./ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.o \
./ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.o 

MAP += \
mipi_csi_ek_ra8p1_ep.map 


# Each subdirectory must supply rules for building sources it contributes
ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/%.o: ../ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m85 -mlittle-endian -mfloat-abi=hard -Os -ffunction-sections -fdata-sections -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -gdwarf-4 -g3 -std=c99 -flax-vector-conversions -fshort-enums -fno-unroll-loops -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\src" -I"." -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra\\fsp\\inc" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra\\fsp\\inc\\api" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra\\fsp\\inc\\instances" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra_gen" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra_cfg\\fsp_cfg\\bsp" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra_cfg\\fsp_cfg" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra\\fsp\\src\\r_mipi_csi" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\mipi_csi_ek_ra8p1_ep\\mipi_csi_ek_ra8p1_ep\\e2studio\\ra\\fsp\\src\\r_vin" -D_RENESAS_RA_ -DUSE_VIRTUAL_COM=0 -D_RA_CORE=CPU0 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

