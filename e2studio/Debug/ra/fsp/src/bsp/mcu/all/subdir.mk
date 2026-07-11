################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/fsp/src/bsp/mcu/all/bsp_clocks.c \
../ra/fsp/src/bsp/mcu/all/bsp_common.c \
../ra/fsp/src/bsp/mcu/all/bsp_delay.c \
../ra/fsp/src/bsp/mcu/all/bsp_group_irq.c \
../ra/fsp/src/bsp/mcu/all/bsp_guard.c \
../ra/fsp/src/bsp/mcu/all/bsp_io.c \
../ra/fsp/src/bsp/mcu/all/bsp_ipc.c \
../ra/fsp/src/bsp/mcu/all/bsp_irq.c \
../ra/fsp/src/bsp/mcu/all/bsp_macl.c \
../ra/fsp/src/bsp/mcu/all/bsp_ospi_b.c \
../ra/fsp/src/bsp/mcu/all/bsp_register_protection.c \
../ra/fsp/src/bsp/mcu/all/bsp_sbrk.c \
../ra/fsp/src/bsp/mcu/all/bsp_sdram.c \
../ra/fsp/src/bsp/mcu/all/bsp_security.c 

C_DEPS += \
./ra/fsp/src/bsp/mcu/all/bsp_clocks.d \
./ra/fsp/src/bsp/mcu/all/bsp_common.d \
./ra/fsp/src/bsp/mcu/all/bsp_delay.d \
./ra/fsp/src/bsp/mcu/all/bsp_group_irq.d \
./ra/fsp/src/bsp/mcu/all/bsp_guard.d \
./ra/fsp/src/bsp/mcu/all/bsp_io.d \
./ra/fsp/src/bsp/mcu/all/bsp_ipc.d \
./ra/fsp/src/bsp/mcu/all/bsp_irq.d \
./ra/fsp/src/bsp/mcu/all/bsp_macl.d \
./ra/fsp/src/bsp/mcu/all/bsp_ospi_b.d \
./ra/fsp/src/bsp/mcu/all/bsp_register_protection.d \
./ra/fsp/src/bsp/mcu/all/bsp_sbrk.d \
./ra/fsp/src/bsp/mcu/all/bsp_sdram.d \
./ra/fsp/src/bsp/mcu/all/bsp_security.d 

CREF += \
mipi_csi_ek_ra8p1_ep.cref 

OBJS += \
./ra/fsp/src/bsp/mcu/all/bsp_clocks.o \
./ra/fsp/src/bsp/mcu/all/bsp_common.o \
./ra/fsp/src/bsp/mcu/all/bsp_delay.o \
./ra/fsp/src/bsp/mcu/all/bsp_group_irq.o \
./ra/fsp/src/bsp/mcu/all/bsp_guard.o \
./ra/fsp/src/bsp/mcu/all/bsp_io.o \
./ra/fsp/src/bsp/mcu/all/bsp_ipc.o \
./ra/fsp/src/bsp/mcu/all/bsp_irq.o \
./ra/fsp/src/bsp/mcu/all/bsp_macl.o \
./ra/fsp/src/bsp/mcu/all/bsp_ospi_b.o \
./ra/fsp/src/bsp/mcu/all/bsp_register_protection.o \
./ra/fsp/src/bsp/mcu/all/bsp_sbrk.o \
./ra/fsp/src/bsp/mcu/all/bsp_sdram.o \
./ra/fsp/src/bsp/mcu/all/bsp_security.o 

MAP += \
mipi_csi_ek_ra8p1_ep.map 


# Each subdirectory must supply rules for building sources it contributes
ra/fsp/src/bsp/mcu/all/%.o: ../ra/fsp/src/bsp/mcu/all/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m85 -mlittle-endian -mfloat-abi=hard -Os -ffunction-sections -fdata-sections -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -gdwarf-4 -g3 -std=c99 -flax-vector-conversions -fshort-enums -fno-unroll-loops -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\src" -I"." -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\fsp\\inc" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\fsp\\inc\\api" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\fsp\\inc\\instances" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra_gen" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra_cfg\\fsp_cfg\\bsp" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra_cfg\\fsp_cfg" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\fsp\\src\\r_mipi_csi" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\fsp\\src\\r_vin" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\fsp\\src\\rm_ethosu" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\tflite-micro" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\ruy" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\gemmlowp" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-driver\\include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\layer_by_layer_profiler\\include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\ethosu_monitor\\include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\ethosu_profiler\\include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\crc\\include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\arm_profiler\\include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-View\\EventRecorder\\Include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-View\\EventRecorder\\Config" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\npu\\flatbuffers\\include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-NN\\Include" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-NN" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-DSP\\PrivateInclude" -I"E:\\Renesas_Cup_Final_Game\\ZIMU\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-DSP\\Include" -D_RENESAS_RA_ -DUSE_VIRTUAL_COM=0 -D_RA_CORE=CPU0 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

