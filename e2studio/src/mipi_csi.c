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
#include "model.h"
#include "yolo_postprocess.h"

#include <stdio.h>
#include <string.h>
#define MAXTRUSTTHRESHOLD (0.50f)
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
static void preprocess_frame_to_yolo(const uint8_t * src, int8_t * dst);
static void draw_rect_rgb565(uint8_t * fb, int x0, int y0, int x1, int y1,
                             uint16_t color, int fb_stride_pixels);
static void draw_text_rgb565(uint8_t * fb, int x, int y, char const * text,
                             uint16_t color, int scale, int fb_stride_pixels);
static void draw_filled_rect_rgb565(uint8_t * fb, int x0, int y0, int x1, int y1,
                                    uint16_t color, int fb_stride_pixels);

static const uint16_t g_yolo_class_colors[YOLO_CLASS_COUNT] =
{
    0xFFE0U, /* Dusty: yellow */
    0xF800U, /* PhysicalDamage: red */
};

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
#if (DISPLAY_OUTPUT == 1U)
    /* GLCDC starts with fb_background[0]. CPU always renders the other buffer. */
    uint8_t draw_buffer_index = 1U;
#endif

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
        uint8_t * p_draw_buffer = fb_background[draw_buffer_index];

        g_vsync_flag = RESET_FLAG;
        /* Wait for a Vsync event */
        while(!g_vsync_flag);

        // 1. 把 VIN 最新帧复制到显示 framebuffer
        if (gp_next_buffer != NULL)
        {
            memcpy(p_draw_buffer, gp_next_buffer, VIN_BYTES_PER_FRAME);
        }

        // 2.填入模型输入
        if (gp_next_buffer != NULL)
        {


            int8_t * model_input = GetModelInputPtr_x();
            preprocess_frame_to_yolo(gp_next_buffer, model_input);//输入图像伸缩预处理

            // 3. 运行模型推理
            RunModel(false);

            int8_t * output = GetModelOutputPtr_Identity_70374();

            yolo_detection_t detections[YOLO_MAX_DETECTIONS];
            int detection_count = yolo_decode_int8_output(output, detections,
                                                           YOLO_MAX_DETECTIONS, MAXTRUSTTHRESHOLD);
            detection_count = yolo_nms(detections, detection_count, 0.45f);

            for (int index = 0; index < detection_count; index++)
            {
                yolo_detection_t const * p_detection = &detections[index];
                uint16_t color = g_yolo_class_colors[p_detection->class_id];
                int x0 = 212 + (int) (p_detection->x1 * 600.0f / YOLO_INPUT_SIZE);
                int y0 =       (int) (p_detection->y1 * 600.0f / YOLO_INPUT_SIZE);
                int x1 = 212 + (int) (p_detection->x2 * 600.0f / YOLO_INPUT_SIZE);
                int y1 =       (int) (p_detection->y2 * 600.0f / YOLO_INPUT_SIZE);
                char label[32];
                unsigned int confidence =
                    (unsigned int) (p_detection->score * 100.0f + 0.5f);
                int text_x;
                int text_y;
                int text_width;

                draw_rect_rgb565(p_draw_buffer, x0, y0, x1, y1, color,
                                 DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0);

                (void) snprintf(label, sizeof(label), "%s:%u",
                                    g_yolo_class_names[p_detection->class_id], confidence);
                text_x = x0;
                text_y = (y0 >= 18) ? (y0 - 18) : (y0 + 2);
                text_width = ((int) strlen(label) * 6 * 2) + 2;

                draw_filled_rect_rgb565(p_draw_buffer, text_x - 1, text_y - 1,
                                         text_x + text_width, text_y + 15,
                                         0x0000U, DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0);
                draw_text_rgb565(p_draw_buffer, text_x, text_y, label, color, 2,
                                  DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0);
            }
            // 4. 读取输出并在 fb_background[0] 上画框
        }

        /*
         * The line-detect interrupt is at the end of active display. Submit
         * here so GLCDC latches the completed back buffer at the immediately
         * following Vsync, before this buffer index is reused.
         */
        g_vsync_flag = RESET_FLAG;
        while(!g_vsync_flag);

        /* Update new frame for GLCDC display */
        err = R_GLCDC_BufferChange(&g_display_ctrl, p_draw_buffer, DISPLAY_FRAME_LAYER_1);
        if (FSP_SUCCESS == err)
        {
            /* The following frame is rendered into the old front buffer. */
            draw_buffer_index ^= 1U;
        }
        else if (FSP_ERR_INVALID_UPDATE_TIMING != err)
        {
            handle_error(err, "** R_GLCDC_BufferChange API FAILED **\r\n");
        }
#endif /* DISPLAY_OUTPUT */
    }

    //ENDWHILE
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
static void preprocess_frame_to_yolo(const uint8_t * src, int8_t * dst)
{
    const int crop_x = 212;
    const int crop_size = 600;
    const int dst_size = 256;
    const int src_stride_bytes = 2048;

    for (int dy = 0; dy < dst_size; dy++)
    {
        int sy = dy * crop_size / dst_size;

        for (int dx = 0; dx < dst_size; dx++)
        {
            int sx = crop_x + dx * crop_size / dst_size;
            int pos = sy * src_stride_bytes + sx * 2;
            uint16_t pixel = (uint16_t) (((uint16_t) src[pos] << 8) | src[pos + 1]);

            uint8_t r = (uint8_t) ((((pixel >> 11) & 0x1FU) * 255U) / 31U);
            uint8_t g = (uint8_t) ((((pixel >>  5) & 0x3FU) * 255U) / 63U);
            uint8_t b = (uint8_t) ((((pixel >>  0) & 0x1FU) * 255U) / 31U);

            int index = (dy * dst_size + dx) * 3;

            /* Edge Impulse RGB input in NHWC layout.  With scale=1/255 and
             * zero_point=-128, INT8 value = RGB_8bit - 128. */
            dst[index + 0] = (int8_t) ((int)r - 128);  /* Edge Impulse RGB input */
            dst[index + 1] = (int8_t) ((int)g - 128);
            dst[index + 2] = (int8_t) ((int)b - 128);
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

static void draw_filled_rect_rgb565(uint8_t * fb, int x0, int y0, int x1, int y1,
                                    uint16_t color, int fb_stride_pixels)
{
    if ((fb == NULL) || (x0 > 1023) || (y0 > 599) || (x1 < 0) || (y1 < 0))
    {
        return;
    }

    if (x0 < 0)    { x0 = 0; }
    if (y0 < 0)    { y0 = 0; }
    if (x1 > 1023) { x1 = 1023; }
    if (y1 > 599)  { y1 = 599; }

    uint16_t * pixels = (uint16_t *) fb;

    for (int y = y0; y <= y1; y++)
    {
        for (int x = x0; x <= x1; x++)
        {
            pixels[y * fb_stride_pixels + x] = color;
        }
    }
}

/* 5x7 glyphs. Only the characters used by the four class names, ':' and the
 * confidence digits are stored. Bit 0 is the top pixel of a glyph column. */
static void font5x7_get_glyph(char character, uint8_t glyph[5])
{
    static const uint8_t unknown[5] = {0x02U, 0x01U, 0x59U, 0x09U, 0x06U};
    static const uint8_t space[5] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    static const uint8_t colon[5] = {0x00U, 0x36U, 0x36U, 0x00U, 0x00U};
    static const uint8_t digit_0[5] = {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU};
    static const uint8_t digit_1[5] = {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U};
    static const uint8_t digit_2[5] = {0x42U, 0x61U, 0x51U, 0x49U, 0x46U};
    static const uint8_t digit_3[5] = {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U};
    static const uint8_t digit_4[5] = {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U};
    static const uint8_t digit_5[5] = {0x27U, 0x45U, 0x45U, 0x45U, 0x39U};
    static const uint8_t digit_6[5] = {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U};
    static const uint8_t digit_7[5] = {0x01U, 0x71U, 0x09U, 0x05U, 0x03U};
    static const uint8_t digit_8[5] = {0x36U, 0x49U, 0x49U, 0x49U, 0x36U};
    static const uint8_t digit_9[5] = {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU};
    static const uint8_t letter_B[5] = {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U};
    static const uint8_t letter_C[5] = {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U};
    static const uint8_t letter_D[5] = {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU};
    static const uint8_t letter_P[5] = {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U};
    static const uint8_t letter_a[5] = {0x20U, 0x54U, 0x54U, 0x54U, 0x78U};
    static const uint8_t letter_c[5] = {0x38U, 0x44U, 0x44U, 0x44U, 0x20U};
    static const uint8_t letter_d[5] = {0x38U, 0x44U, 0x44U, 0x48U, 0x7FU};
    static const uint8_t letter_e[5] = {0x38U, 0x54U, 0x54U, 0x54U, 0x18U};
    static const uint8_t letter_i[5] = {0x00U, 0x44U, 0x7DU, 0x40U, 0x00U};
    static const uint8_t letter_k[5] = {0x7FU, 0x10U, 0x28U, 0x44U, 0x00U};
    static const uint8_t letter_l[5] = {0x00U, 0x41U, 0x7FU, 0x40U, 0x00U};
    static const uint8_t letter_n[5] = {0x7CU, 0x08U, 0x04U, 0x04U, 0x78U};
    static const uint8_t letter_o[5] = {0x38U, 0x44U, 0x44U, 0x44U, 0x38U};
    static const uint8_t letter_p[5] = {0xFCU, 0x24U, 0x24U, 0x24U, 0x18U};
    static const uint8_t letter_r[5] = {0x7CU, 0x08U, 0x04U, 0x04U, 0x08U};
    static const uint8_t letter_s[5] = {0x48U, 0x54U, 0x54U, 0x54U, 0x20U};
    static const uint8_t letter_t[5] = {0x04U, 0x3FU, 0x44U, 0x40U, 0x20U};
    static const uint8_t letter_u[5] = {0x3CU, 0x40U, 0x40U, 0x20U, 0x7CU};
    static const uint8_t letter_y[5] = {0x0CU, 0x50U, 0x50U, 0x50U, 0x3CU};
    const uint8_t * p_glyph = unknown;

    switch (character)
    {
        case ' ': p_glyph = space; break;
        case ':': p_glyph = colon; break;
        case '0': p_glyph = digit_0; break;
        case '1': p_glyph = digit_1; break;
        case '2': p_glyph = digit_2; break;
        case '3': p_glyph = digit_3; break;
        case '4': p_glyph = digit_4; break;
        case '5': p_glyph = digit_5; break;
        case '6': p_glyph = digit_6; break;
        case '7': p_glyph = digit_7; break;
        case '8': p_glyph = digit_8; break;
        case '9': p_glyph = digit_9; break;
        case 'B': p_glyph = letter_B; break;
        case 'C': p_glyph = letter_C; break;
        case 'D': p_glyph = letter_D; break;
        case 'P': p_glyph = letter_P; break;
        case 'a': p_glyph = letter_a; break;
        case 'c': p_glyph = letter_c; break;
        case 'd': p_glyph = letter_d; break;
        case 'e': p_glyph = letter_e; break;
        case 'i': p_glyph = letter_i; break;
        case 'k': p_glyph = letter_k; break;
        case 'l': p_glyph = letter_l; break;
        case 'n': p_glyph = letter_n; break;
        case 'o': p_glyph = letter_o; break;
        case 'p': p_glyph = letter_p; break;
        case 'r': p_glyph = letter_r; break;
        case 's': p_glyph = letter_s; break;
        case 't': p_glyph = letter_t; break;
        case 'u': p_glyph = letter_u; break;
        case 'y': p_glyph = letter_y; break;
        default: break;
    }

    for (int column = 0; column < 5; column++)
    {
        glyph[column] = p_glyph[column];
    }
}

static void draw_text_rgb565(uint8_t * fb, int x, int y, char const * text,
                             uint16_t color, int scale, int fb_stride_pixels)
{
    if ((fb == NULL) || (text == NULL) || (scale <= 0))
    {
        return;
    }

    uint16_t * pixels = (uint16_t *) fb;

    while (*text != '\0')
    {
        uint8_t glyph[5];
        font5x7_get_glyph(*text, glyph);

        for (int column = 0; column < 5; column++)
        {
            for (int row = 0; row < 7; row++)
            {
                if ((glyph[column] & (1U << row)) == 0U)
                {
                    continue;
                }

                for (int scale_y = 0; scale_y < scale; scale_y++)
                {
                    int pixel_y = y + row * scale + scale_y;

                    if ((pixel_y < 0) || (pixel_y >= 600))
                    {
                        continue;
                    }

                    for (int scale_x = 0; scale_x < scale; scale_x++)
                    {
                        int pixel_x = x + column * scale + scale_x;

                        if ((pixel_x >= 0) && (pixel_x < 1024))
                        {
                            pixels[pixel_y * fb_stride_pixels + pixel_x] = color;
                        }
                    }
                }
            }
        }

        x += 6 * scale;
        text++;
    }
}
