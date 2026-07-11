/***********************************************************************************************************************
 * File Name    : mipi_csi.c
 * Description  : Contains data structures and functions used in hal_entry.c.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/

#include "mipi_csi.h"
#include "model.h"          // RunModel(), GetModelInputPtr_xxx(), GetModelOutputPtr_xxx()
#include "sub_0000_tensors.h"  // kArenaSize_sub_0000

/* External variables */
extern sensor_reg_t live_camera;
extern const camera_config_t camera_profiles[RES_MAX];

extern const vin_extended_cfg_t g_vin_cfg_extend;

extern volatile uint8_t g_vsync_flag;

/* Global variables */
uint8_t * gp_next_buffer;
uint16_t g_image_width = RESET_VALUE;
uint16_t g_image_height = RESET_VALUE;

vin_extended_cfg_t g_vin_cfg_run_time_extend;
capture_cfg_t g_vin_cfg_run_time;

static fsp_err_t vin_camera_start(capture_cfg_t const * p_cfg);
static fsp_err_t vin_scale_image(uint16_t new_width, uint16_t new_height);

/***********************************************************************************************************************
 *  Function Name: mipi_csi_ep_entry
 *  Description  : This function is used to start MIPI CSI example operation.
 *  Arguments    : None
 *  Return Value : None
 **********************************************************************************************************************/
void mipi_csi_ep_entry(void)
{
    fsp_pack_version_t  version = {RESET_VALUE};
    fsp_err_t           err     = FSP_SUCCESS;

    /* Initialize the terminal */
    TERM_INIT();

    /* Version get API for FSP pack information */
    R_FSP_VersionGet(&version);

    /* Example Project information printed on the RTT */
    APP_PRINT (BANNER_INFO, EP_VERSION, version.version_id_b.major, version.version_id_b.minor, version.version_id_b.patch);
    APP_PRINT (EP_INFO);

    /* Initialize IIC module to control the switch onboard and the camera sensor */
    err = i2c_control_init();
    handle_error(err, "i2c_control_init FAILED \r\n");
    R_BSP_SoftwareDelay(10,BSP_DELAY_UNITS_MILLISECONDS);/*New delay here at 2026/6/2 */

    /* Setup the camera with MIPI CSI configuration */
    err = camera_open();
    handle_error(err, " ** camera_open FAILED ** \r\n");

    /* 硬编码: 分辨率 = 1024x600 */
    g_image_width  = camera_profiles[RES_VGA].width;
    g_image_height = camera_profiles[RES_VGA].height;

    /*If_GLCDC_On*/
#if (DISPLAY_OUTPUT == 1U)
    R_BSP_SoftwareDelay(10,BSP_DELAY_UNITS_MILLISECONDS);/*New delay here at 2026/6/2 */
    /* Initialize GLCDC with the selected resolution */
    err = glcdc_init();
    handle_error(err, "glcdc_init FAILED \r\n");
    R_BSP_SoftwareDelay(10,BSP_DELAY_UNITS_MILLISECONDS);/*New delay here at 2026/6/2 */
#endif /* DISPLAY_OUTPUT */

    /* Clear old images in SDRAM */
    memset(vin_image_buffer_1, RESET_VALUE, VIN_BYTES_PER_FRAME);
    memset(vin_image_buffer_2, RESET_VALUE, VIN_BYTES_PER_FRAME);
    memset(vin_image_buffer_3, RESET_VALUE, VIN_BYTES_PER_FRAME);

    
    /* Scale the output image via VIN module */
    err = vin_scale_image(g_image_width, g_image_height);
    handle_error(err, "vin_scale_image FAILED \r\n");
    R_BSP_SoftwareDelay(10,BSP_DELAY_UNITS_MILLISECONDS);/*New delay here at 2026/6/2 */

    /* Start VIN with run-time configuration */
    err = vin_camera_start(&g_vin_cfg_run_time);
    handle_error(err, " ** vin_camera_start FAILED ** \r\n");
    R_BSP_SoftwareDelay(10,BSP_DELAY_UNITS_MILLISECONDS);/*New delay here at 2026/6/2 */

    /* ====== 硬编码: Live Camera 模式 ====== */
    err = camera_write_array(&live_camera);
    handle_error(err, "Change to live camera mode FAILED \r\n");
    APP_PRINT("\r\nLive camera streaming started\r\n");
    R_BSP_SoftwareDelay(10,BSP_DELAY_UNITS_MILLISECONDS);

    /* ====== NPU 初始化 ====== */
    err = RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);
    handle_error(err, "RM_ETHOSU_Open FAILED\r\n");
    R_BSP_SoftwareDelay(10,BSP_DELAY_UNITS_MILLISECONDS);

        /* ====== 主循环: Vsync → 刷新显存 ====== */
    while(true)
    {
#if (DISPLAY_OUTPUT == 1U)
        g_vsync_flag = RESET_FLAG;
        /* Wait for a Vsync event */
        while(!g_vsync_flag);

        // 1. 把 VIN 最新帧复制到显示 framebuffer
        if (gp_next_buffer != NULL)
        {
            memcpy(fb_background[0], gp_next_buffer, VIN_BYTES_PER_FRAME);
        }

        // 2.填入模型输入
        if (gp_next_buffer != NULL)
        {
            int8_t *model_input = GetModelInputPtr_serving_default_x_0();
            preprocess_frame_to_fomo(gp_next_buffer, model_input);

            // 3. 运行模型推理
            RunModel(true);

            // 4. 读取输出并在 fb_background[0] 上画框
            int8_t *output = GetModelOutputPtr_StatefulPartitionedCall_0_70066();

            const int8_t DETECT_THRESHOLD = 60;  // int8格式，0对应量化后的中间值，可调
            const int BOX_HALF = 20;  // 框的半径（像素，在1024x600坐标系中）

            for (int row = 0; row < 32; row++) {
                for (int col = 0; col < 32; col++) {
                    for (int cls = 1; cls < 4; cls++) {  // 跳过背景cls=0
                        int8_t score = output[row * 32 * 4 + col * 4 + cls];
                        if (score > DETECT_THRESHOLD) {
                            // 映射到 fb 坐标
                            int cx = 212 + (col * 8 + 4) * 600 / 256;
                            int cy =   0 + (row * 8 + 4) * 600 / 256;
                            // 画红色框 (RGB565红色 = 0xF800)
                            draw_rect_rgb565(fb_background[0],
                                             cx - BOX_HALF, cy - BOX_HALF,
                                             cx + BOX_HALF, cy + BOX_HALF,
                                             0xF800,
                                             DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0);
                            APP_PRINT("Detected cls=%d at grid(%d,%d)\r\n", cls, col, row);
                        }
                    }
                }
            }
        }


        /* Update new frame for GLCDC display */
        err = R_GLCDC_BufferChange(&g_display_ctrl, (uint8_t * const) fb_background[0], DISPLAY_FRAME_LAYER_1);
        if (FSP_ERR_INVALID_UPDATE_TIMING != err)
        {
            handle_error(err, "** R_GLCDC_BufferChange API FAILED **\r\n");
        }
#endif /* DISPLAY_OUTPUT */
    }
}
/***********************************************************************************************************************
* End of function mipi_csi_ep_entry
***********************************************************************************************************************/

/***********************************************************************************************************************
 *  Function Name: vin_callback
 *  Description  : This function is used to get captured image from VIN callback
 *  Arguments    : p_args       Pointer to callback argument
 *  Return Value : None
 **********************************************************************************************************************/
void vin_callback (capture_callback_args_t * p_args)
 {
     vin_module_status_t    module_status    = (vin_module_status_t) p_args->event_status;
     vin_interrupt_status_t interrupt_status = (vin_interrupt_status_t) p_args->interrupt_status;
     FSP_PARAMETER_NOT_USED(module_status);

     switch (p_args->event)
     {
         case VIN_EVENT_NOTIFY:
         {
             if (interrupt_status.bits.frame_complete)
             {
                 gp_next_buffer = p_args->p_buffer;
             }

             break;
         }

         case VIN_EVENT_ERROR:
         {
             break;
         }

         default:
         {
             /* Do nothing */
             break;
         }
     }
 }
/***********************************************************************************************************************
* End of function vin_callback
***********************************************************************************************************************/

/***********************************************************************************************************************
 *  Function Name: mipi_csi0_callback
 *  Description  : This function is used to handle MIPI CSI event
 *  Arguments    : p_args      Pointer to callback argument
 *  Return Value : None
 **********************************************************************************************************************/
 void mipi_csi0_callback (mipi_csi_callback_args_t * p_args)
 {
     switch (p_args->event)
     {
         /*
          * In this project, the application does not rely on any MIPI CSI events.
          * FRAME_DATA event is ignored because VIN automatically handles captured frames.
          * DATA_LANE event status changes are not monitored (stable single-lane setup).
          * VIRTUAL_CHANNEL event is unused as only one channel 0 is active.
          * POWER and SHORT_PACKET_FIFO events are ignored for simplicity.
          */
         case MIPI_CSI_EVENT_DATA_LANE:
         case MIPI_CSI_EVENT_FRAME_DATA:
         case MIPI_CSI_EVENT_POWER:
         case MIPI_CSI_EVENT_SHORT_PACKET_FIFO:
         case MIPI_CSI_EVENT_VIRTUAL_CHANNEL:
             break;

         default:
             break;
     }
 }
 /***********************************************************************************************************************
* End of function mipi_csi0_callback
***********************************************************************************************************************/

/***********************************************************************************************************************
 *  Function Name: vin_camera_start
 *  Description  : This function is used to initialize the VIN module with inputed configuration
 *  Arguments    : p_cfg          Pointer to selected VIN configuration
 *  Return Value : FSP_SUCCESS    Upon successful operation
 *                 Any Other Error code apart from FSP_SUCCESS
 **********************************************************************************************************************/
static fsp_err_t vin_camera_start(capture_cfg_t const * p_cfg)
 {
     fsp_err_t err;
     //APP_PRINT("\r\n Test_point \r\n");
     /* Sanity-check the input pointer */
     if (p_cfg == NULL)
     {
         return FSP_ERR_INVALID_ARGUMENT;
     }
     //APP_PRINT("\r\n Test_point \r\n");
     /* Stop streaming before de-initialing VIN module */
     err = camera_stream_off();
     APP_ERR_RET( FSP_SUCCESS != err, err, " ** camera_stream_off FAILED ** \r\n");
     //APP_PRINT("\r\n camera_stream_off FAILED \r\n");
     /* De-Initialize VIN module if it was opened */
     if (MODULE_CLOSE != g_vin_ctrl.open)
     {
         err = R_VIN_Close(&g_vin_ctrl);
         APP_ERR_RET( FSP_SUCCESS != err, err, " ** R_VIN_Close API FAILED ** \r\n");
     }

     /* Power down camera after de-initialing VIN module */
     err = write_reg_16bit(SYS_CTRL0_REG, SYS_CTRL0_SW_PWDN);
     APP_ERR_RET( FSP_SUCCESS != err, err, "write_reg_16bit to power down camera failed\r\n");

     /* Initialize VIN with the provided configuration */
     err = R_VIN_Open(&g_vin_ctrl, p_cfg);
     APP_ERR_RET( FSP_SUCCESS != err, err, " ** R_VIN_Open API FAILED ** \r\n");

     /* Start VIN capture engine */
     err = R_VIN_CaptureStart(&g_vin_ctrl, NULL);
     APP_ERR_RET( FSP_SUCCESS != err, err, " ** R_VIN_CaptureStart API FAILED ** \r\n");

     /* Start the camera sensor streaming */
     err = camera_stream_on();
     APP_ERR_RET( FSP_SUCCESS != err, err, " ** camera_stream_on FAILED ** \r\n");

     return err;
 }
/***********************************************************************************************************************
* End of function vin_camera_start
***********************************************************************************************************************/

/***********************************************************************************************************************
 *  Function Name: vin_scale_image
 *  Description  : This function is used to change the VIN parameters according to the inputed size
 *  Arguments    : new_width      Target width of image
 *                 new_height     Target height of image
 *  Return Value : FSP_SUCCESS    Upon successful operation
 *                 Any Other Error code apart from FSP_SUCCESS
 **********************************************************************************************************************/
static fsp_err_t vin_scale_image(uint16_t new_width, uint16_t new_height)
 {
     /* Validate arguments (avoid division by zero later) */
     if ((new_width == 0U) || (new_height == 0U))
     {
         return FSP_ERR_INVALID_ARGUMENT;
     }

     /* Copy the default extended configuration into the runtime copy */
     g_vin_cfg_run_time_extend = g_vin_cfg_extend;

     /* Fetch old output dimensions and masks from the default configuration */
     const uint32_t old_v  = (uint32_t) g_vin_cfg_extend.conversion_data.uds_clipping_bits.cl_vsize;
     const uint32_t old_h  = (uint32_t) g_vin_cfg_extend.conversion_data.uds_clipping_bits.cl_hsize;
     const uint16_t old_vm = (uint16_t) g_vin_cfg_extend.conversion_data.uds_scale_bits.vertical_mask;
     const uint16_t old_hm = (uint16_t) g_vin_cfg_extend.conversion_data.uds_scale_bits.horizontal_mask;

     /* Fetch input dimensions from the default configuration */
     const uint16_t input_height = (uint16_t) g_vin_cfg_extend.input_ctrl.preclip.line_end + 1;
     const uint16_t input_width = (uint16_t) g_vin_cfg_extend.input_ctrl.preclip.pixel_end + 1;

     /* Recalculate the scale masks: new_mask = (old_mask * old_size) / new_size */
     g_vin_cfg_run_time_extend.conversion_data.uds_scale_bits.vertical_mask =
         (uint16_t) ( ((uint32_t) old_vm * old_v) / new_height );

     g_vin_cfg_run_time_extend.conversion_data.uds_scale_bits.horizontal_mask =
         (uint16_t) ( ((uint32_t) old_hm * old_h) / new_width );

     /* Update clipping sizes to match the new target dimensions */
     g_vin_cfg_run_time_extend.conversion_data.uds_clipping_bits.cl_vsize = (uint16_t) new_height;
     g_vin_cfg_run_time_extend.conversion_data.uds_clipping_bits.cl_hsize = (uint16_t) new_width;

     /* Update scaling enable bit */
     if ((input_height != new_height) || (input_width != new_width))
     {
         g_vin_cfg_run_time_extend.input_ctrl.cfg_bits.scaling_enable = true;
     }
     else
     {
         g_vin_cfg_run_time_extend.input_ctrl.cfg_bits.scaling_enable = false;
     }

     /* Copy the default main configuration into the runtime copy */
     g_vin_cfg_run_time = g_vin_cfg;

     /* Point p_extend at the freshly updated runtime-extended configuration */
     g_vin_cfg_run_time.p_extend = &g_vin_cfg_run_time_extend;

     return FSP_SUCCESS;
 }
/***********************************************************************************************************************
* End of function vin_scale_image
***********************************************************************************************************************/

/***********************************************************************************************************************
 *  Function Name: handle_error
 *  Description  : This function close all opened modules, print and trap error.
 *  Arguments    : err            error code
 *                 err_str        error string
 *  Return Value : None
 **********************************************************************************************************************/
void handle_error (fsp_err_t err, char * err_str)
{
    if(FSP_SUCCESS != err)
    {
        /* Print the error */
        APP_PRINT(err_str);

        /* Close opened VIN module*/
        if(0U != g_vin_ctrl.open)
        {
            if(FSP_SUCCESS != R_VIN_Close(&g_vin_ctrl))
            {
                APP_ERR_PRINT("R_VIN_Close FAILED\r\n");
            }
        }

        /* Close opened I2C Master module*/
        if(0U != g_i2c_master_for_peripheral_ctrl.open)
        {
            if(FSP_SUCCESS != R_IIC_MASTER_Close(&g_i2c_master_for_peripheral_ctrl))
            {
                APP_ERR_PRINT("R_IIC_MASTER_Close FAILED\r\n");
            }
        }

        /* Trap the error */
        APP_ERR_TRAP(err);
    }
}
/***********************************************************************************************************************
* End of function handle_error
***********************************************************************************************************************/


// 把 VIN 的 RGB565 1024x600 帧 → FOMO 输入 int8 RGB 256x256
// src: VIN帧缓冲指针(RGB565, stride=2048字节)
// dst: 模型输入指针(RGB888 int8, 256x256x3)
static void preprocess_frame_to_fomo(const uint8_t *src, int8_t *dst)
{
    // 从 1024x600 中心裁剪出 600x600，再缩放到 256x256
    // 中心裁剪起点：x_offset = (1024 - 600) / 2 = 212
    const int src_crop_x = 212;
    const int src_crop_y = 0;
    const int src_crop_size = 600;   // 正方形裁剪区域

    const int dst_size = 256;
    const int src_stride_bytes = 2048;  // VIN_CFG_BYTES_PER_LINE

    for (int dy = 0; dy < dst_size; dy++)
    {
        // 在源裁剪区域中对应的行
        int sy = src_crop_y + (dy * src_crop_size / dst_size);

        for (int dx = 0; dx < dst_size; dx++)
        {
            // 在源裁剪区域中对应的列
            int sx = src_crop_x + (dx * src_crop_size / dst_size);

            // 读 RGB565 像素（注意大端：byte_swap=1）
            int byte_offset = sy * src_stride_bytes + sx * 2;
            uint16_t pixel = ((uint16_t)src[byte_offset] << 8)
                           | ((uint16_t)src[byte_offset + 1]);

            // 提取 RGB 分量，展开到 8bit
            uint8_t r5 = (pixel >> 11) & 0x1F;
            uint8_t g6 = (pixel >> 5)  & 0x3F;
            uint8_t b5 = (pixel >> 0)  & 0x1F;

            uint8_t r = (uint8_t)((r5 << 3) | (r5 >> 2));
            uint8_t g = (uint8_t)((g6 << 2) | (g6 >> 4));
            uint8_t b = (uint8_t)((b5 << 3) | (b5 >> 2));

            // 写入 int8（uint8 - 128），RGB顺序
            int dst_offset = (dy * dst_size + dx) * 3;
            dst[dst_offset + 0] = (int8_t)((int16_t)r - 128);
            dst[dst_offset + 1] = (int8_t)((int16_t)g - 128);
            dst[dst_offset + 2] = (int8_t)((int16_t)b - 128);
        }
    }
}

static void draw_rect_rgb565(uint8_t *fb, int x0, int y0, int x1, int y1,
                              uint16_t color, int fb_stride_pixels)
{
    // 越界保护
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= 1024) x1 = 1023;
    if (y1 >= 600)  y1 = 599;

    uint16_t *pixels = (uint16_t *)fb;

    // 上边和下边（水平线）
    for (int x = x0; x <= x1; x++) {
        pixels[y0 * fb_stride_pixels + x] = color;
        pixels[y1 * fb_stride_pixels + x] = color;
    }
    // 左边和右边（竖线）
    for (int y = y0; y <= y1; y++) {
        pixels[y * fb_stride_pixels + x0] = color;
        pixels[y * fb_stride_pixels + x1] = color;
    }
}
