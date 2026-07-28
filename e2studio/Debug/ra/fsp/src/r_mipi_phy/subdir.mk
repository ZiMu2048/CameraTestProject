################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/fsp/src/r_mipi_phy/r_mipi_phy.c 

C_DEPS += \
./ra/fsp/src/r_mipi_phy/r_mipi_phy.d 

CREF += \
RA8P1_CAM_GLCDC_AI.cref 

OBJS += \
./ra/fsp/src/r_mipi_phy/r_mipi_phy.o 

MAP += \
RA8P1_CAM_GLCDC_AI.map 


# Each subdirectory must supply rules for building sources it contributes
ra/fsp/src/r_mipi_phy/%.o: ../ra/fsp/src/r_mipi_phy/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m85 -mlittle-endian -mfloat-abi=hard -Os -ffunction-sections -fdata-sections -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -gdwarf-4 -g3 -std=c99 -flax-vector-conversions -fshort-enums -fno-unroll-loops -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\src" -I"." -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\fsp\\inc" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\fsp\\inc\\api" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\fsp\\inc\\instances" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra_gen" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra_cfg\\fsp_cfg\\bsp" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra_cfg\\fsp_cfg" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\fsp\\src\\r_mipi_csi" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\fsp\\src\\r_vin" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-DSP\\PrivateInclude" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-DSP\\Include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\fsp\\src\\rm_ethosu" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\tflite-micro" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\ruy" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\gemmlowp" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-driver\\include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\layer_by_layer_profiler\\include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\ethosu_monitor\\include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\ethosu_profiler\\include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\crc\\include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\ethos-u-core-software\\lib\\arm_profiler\\include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-View\\EventRecorder\\Include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-View\\EventRecorder\\Config" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\npu\\flatbuffers\\include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-NN\\Include" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\arm\\CMSIS-NN" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\src\\model" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\tes\\dave2d\\inc" -I"D:\\Lab\\Lab_MCU\\Renesas_RA\\RA8P1_CAM_GLCDC_AI\\CameraTestProject\\e2studio\\ra\\fsp\\src\\r_drw" -D_RENESAS_RA_ -DUSE_VIRTUAL_COM=0 -D_RA_CORE=CPU0 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

