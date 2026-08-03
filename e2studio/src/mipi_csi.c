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
#include "dave2D_overlay.h"
#include "SEGGER_RTT/bsp_print.h"
#include "DA16200/da16200_AT.h"

#include <stdio.h>
#include <string.h>

#define MAXTRUSTTHRESHOLD          (0.50f)//最大置信度

/* 检测框边框宽度，单位为显示像素。 */
#define DAVE2D_BOX_LINE_WIDTH      (2)

/* 5×7 标签字模的整数放大倍数。 */
#define DAVE2D_TEXT_SCALE          (2)

#define DA16200_WIFI_SSID                 "527_RA8P1"
#define DA16200_WIFI_PASSWORD             "060117klj"
#define DA16200_WIFI_CONNECT_TIMEOUT_MS   (60000U)

#define DA16200_TCP_SERVER_IP      "192.168.137.1"
#define DA16200_TCP_SERVER_PORT    (5000U)

static uint8_t g_da16200_tcp_cid = 0xFFU;


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

static const uint16_t g_yolo_class_colors[YOLO_CLASS_COUNT] =
{
    0xF800U, /* PhysicalDamage: red */
};

static fsp_err_t da16200_connect_local_wifi(void);
static fsp_err_t da16200_connect_local_tcp(void);
/*
 * 功能：按顺序发送最小 AT 指令集，确认 UART、AT 解释器和 Wi-Fi 状态查询链路正常。
 * 调用环境：系统初始化阶段调用一次，不可在中断中调用。
 * 返回值：全部指令收到 OK 时返回 FSP_SUCCESS，否则返回首条失败指令的错误码。
 */
static fsp_err_t da16200_ensure_station_mode(void)
{
    static char response[DA16200_STR_LEN_512];
    const char * p_mode;
    fsp_err_t err;

    memset(response, 0, sizeof(response));
    err = DA16200_SendCommandAndGetResponse("AT+CWMODE=?\r\n",
                                            response,
                                            (uint16_t) sizeof(response),
                                            5000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    p_mode = strstr(response, "+CWMODE:");
    if ((NULL == p_mode) || (p_mode[8] < '0') || (p_mode[8] > '2'))
    {
        g_printf("DA16200: invalid CWMODE response\r\n");
        return FSP_ERR_ASSERTION;
    }

    if ('0' == p_mode[8])
    {
        g_printf("DA16200: Station mode already active\r\n");
        return FSP_SUCCESS;
    }

    g_printf("DA16200: switching Wi-Fi mode %c -> 0 (Station)\r\n", p_mode[8]);

    memset(response, 0, sizeof(response));
    err = DA16200_SendCommandAndGetResponse("AT+CWMODE=0\r\n",
                                            response,
                                            (uint16_t) sizeof(response),
                                            5000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    memset(response, 0, sizeof(response));
    err = DA16200_SendCommandAndGetResponse("AT+RST\r\n",
                                            response,
                                            (uint16_t) sizeof(response),
                                            5000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    R_BSP_SoftwareDelay(3U, BSP_DELAY_UNITS_SECONDS);

    memset(response, 0, sizeof(response));
    err = DA16200_SendCommandAndGetResponse("AT\r\n",
                                            response,
                                            (uint16_t) sizeof(response),
                                            3000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    memset(response, 0, sizeof(response));
    err = DA16200_SendCommandAndGetResponse("AT+CWMODE=?\r\n",
                                            response,
                                            (uint16_t) sizeof(response),
                                            5000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    p_mode = strstr(response, "+CWMODE:");
    if ((NULL == p_mode) || ('0' != p_mode[8]))
    {
        g_printf("DA16200: Station mode verification failed\r\n");
        return FSP_ERR_ASSERTION;
    }

    g_printf("DA16200: Station mode enabled\r\n");
    return FSP_SUCCESS;
}

static fsp_err_t da16200_protocol_probe(void)
{
    static char response[DA16200_STR_LEN_512];
    static const char * const probe_commands[] =
    {
        "AT\r\n",
        "AT+SDKVER\r\n",
    };
    static const char * const station_status_commands[] =
    {
        "AT+CWSTA\r\n",
        "AT+CWSTAT\r\n",
    };
    fsp_err_t err;

    for (size_t index = 0U;
         index < (sizeof(probe_commands) / sizeof(probe_commands[0]));
         index++)
    {
        memset(response, 0, sizeof(response));
        g_printf("\r\nDA16200 probe command %u\r\n", (unsigned int) index);
        err = DA16200_SendCommandAndGetResponse(probe_commands[index],
                                                response,
                                                (uint16_t) sizeof(response),
                                                5000U);
        if (FSP_SUCCESS != err)
        {
            g_printf("DA16200 probe failed at command %u, err=%d\r\n",
                     (unsigned int) index,
                     err);
            return err;
        }
    }

    err = da16200_ensure_station_mode();
    if (FSP_SUCCESS != err)
    {
        g_printf("DA16200 Station mode setup failed, err=%d\r\n", err);
        return err;
    }

    for (size_t index = 0U;
         index < (sizeof(station_status_commands) / sizeof(station_status_commands[0]));
         index++)
    {
        memset(response, 0, sizeof(response));
        g_printf("\r\nDA16200 station status command %u\r\n", (unsigned int) index);
        err = DA16200_SendCommandAndGetResponse(station_status_commands[index],
                                                response,
                                                (uint16_t) sizeof(response),
                                                5000U);
        if (FSP_SUCCESS != err)
        {
            g_printf("DA16200 station status failed at command %u, err=%d\r\n",
                     (unsigned int) index,
                     err);
            return err;
        }
    }

    g_printf("\r\nDA16200 protocol probe passed\r\n");
    return FSP_SUCCESS;
}

static fsp_err_t da16200_connect_local_wifi(void)
{
    fsp_err_t err;
    static char response[DA16200_STR_LEN_512];

    err = DA16200_EnsureWifiConnected(DA16200_WIFI_SSID,
                                      DA16200_WIFI_PASSWORD,
                                      DA16200_WIFI_CONNECT_TIMEOUT_MS);
    if(FSP_SUCCESS !=err)
    {
        return err;
    }

    memset(response, 0, sizeof(response));
    err = DA16200_SendCommandAndGetResponse("AT+CWSTAT\r\n",
                                            response,
                                            (uint16_t) sizeof(response),
                                            5000U);
    
    if (FSP_SUCCESS != err)
    {
        g_printf("DA16200: CWSTAT query failed, err=%d\r\n",
                 err);
        return err;
    }

    if (NULL == strstr(response, "wpa_state=COMPLETED"))
    {
        g_printf("DA16200: Wi-Fi state is not COMPLETED\r\n");
        return FSP_ERR_ASSERTION;
    }
    
    g_printf("DA16200: local Wi-Fi is ready\r\n");
    return FSP_SUCCESS;
}

static fsp_err_t da16200_connect_local_tcp(void)
{
    fsp_err_t err;

    err = DA16200_TcpClientOpen(DA16200_TCP_SERVER_IP,
                                DA16200_TCP_SERVER_PORT,
                                &g_da16200_tcp_cid);
    if (FSP_SUCCESS != err)
    {
        g_printf("DA16200: TCP client open failed, err=%d\r\n", err);
        return err;
    }

    g_printf("DA16200: TCP client connected, CID=%u\r\n",
             (unsigned int) g_da16200_tcp_cid);
    return FSP_SUCCESS;
}

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

    g_printf("HELLOWORLD\r\n");

    /* 初始化 SCI6，并在模块上电稳定后执行最小 AT 协议探测。 */
    err = DA16200_UartInit();
    if (FSP_SUCCESS != err)
    {
        g_printf("DA16200 UART init failed, camera continues, err=%d\r\n", err);
    }
    else
    {
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_SECONDS);
        err = da16200_protocol_probe();
        if (FSP_SUCCESS != err)
        {
            g_printf("DA16200 is unavailable, camera continues, err=%d\r\n",
                    err);
        }
        else
        {
            err = da16200_connect_local_wifi();
            if (FSP_SUCCESS != err)
            {
                g_printf("DA16200 Wi-Fi unavailable, camera continues, err=%d\r\n",
                        err);
            }

            err = DA16200_TcpCloseAll();
            if (FSP_SUCCESS != err)
            {
                g_printf("CID Close Failed");
            }

            err = da16200_connect_local_tcp();
            if (FSP_SUCCESS != err)
            {
                g_printf("DA16200 TCP client unavailable, camera continues, err=%d\r\n",
                        err);
            }
            err = DA16200_TcpClientSendText(g_da16200_tcp_cid,
                                            "HELLO FROM RA8P1");
            if (FSP_SUCCESS != err)
            {
                g_printf("DA16200: TCP text send failed, err=%d\r\n",
                        (int) err);
            }

            g_printf("DA16200: TCP text command transmitted\r\n");
            
        }
    }

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

#if (DISPLAY_OUTPUT == 1U)
    if (!dave2d_overlay_init())
    {
        int32_t d2_error = dave2d_overlay_get_last_error();

        (void) d2_error;

        handle_error(FSP_ERR_INTERNAL,
                    "** DAVE 2D INITIALIZATION FAILED **\r\n");
    }
    R_BSP_SoftwareDelay(10,BSP_DELAY_UNITS_MILLISECONDS);
#endif

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
        /*双缓冲*/
        uint8_t * p_draw_buffer = fb_background[draw_buffer_index];

        /*垂直同步*/
        g_vsync_flag = RESET_FLAG;
        while(!g_vsync_flag);


        /*把 VIN 最新帧复制到显示 framebuffer*/
        if (gp_next_buffer != NULL)
        {
            memcpy(p_draw_buffer, gp_next_buffer, VIN_BYTES_PER_FRAME);
        }

        /*算子工作*/
        if (gp_next_buffer != NULL)
        {
            int8_t * model_input = GetModelInputPtr_x();//获取模型输入缓冲区指针
            preprocess_frame_to_yolo(gp_next_buffer, model_input);//输入图像伸缩预处理

            RunModel(false);//调用模型推理

            int8_t * output = GetModelOutputPtr_Identity_70374();//获取模型输出缓冲区指针

            yolo_detection_t detections[YOLO_MAX_DETECTIONS];
            int detection_count = yolo_decode_int8_output(output, detections,
                                                           YOLO_MAX_DETECTIONS, MAXTRUSTTHRESHOLD);//解码模型输出，得到检测框
            detection_count = yolo_nms(detections, detection_count, 0.45f);//非极大值抑制，去除重叠框

            /*
             * 一帧只打开和提交一次 D/AVE 2D render buffer。
             * 检测框、标签背景和文字都只向同一张命令表追加命令。
             */
            if (detection_count > 0)
            {
                bool d2_ok =
                    dave2d_overlay_begin(p_draw_buffer,
                                         (int) CAMERA_IMAGE_WIDTH,
                                         (int) CAMERA_IMAGE_HEIGHT,
                                         DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0);

                if (!d2_ok)
                {
                    int32_t d2_error = dave2d_overlay_get_last_error();
                    (void) d2_error;
                    handle_error(FSP_ERR_INTERNAL,
                                 "** DAVE 2D BEGIN FAILED **\r\n");
                }

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

                    /*
                     * 每个字符占 6×7 个字模单位，标签优先放在检测框上方。
                     */
                    (void) snprintf(label,
                                    sizeof(label),
                                    "%s:%u",
                                    g_yolo_class_names[p_detection->class_id],
                                    confidence);
                    text_x = x0;
                    text_y = (y0 >= 18) ? (y0 - 18) : (y0 + 2);
                    text_width = ((int) strlen(label) * 6 * DAVE2D_TEXT_SCALE) + 2;

                    d2_ok = dave2d_overlay_draw_rect(x0,
                                                     y0,
                                                     x1,
                                                     y1,
                                                     color,
                                                     DAVE2D_BOX_LINE_WIDTH);
                    if (!d2_ok)
                    {
                        int32_t d2_error = dave2d_overlay_get_last_error();
                        (void) d2_error;
                        handle_error(FSP_ERR_INTERNAL,
                                     "** DAVE 2D DRAW RECT FAILED **\r\n");
                    }

                    /*
                     * 黑色标签背景也追加到当前 render buffer。
                     * 此处仍然只录制命令，真正执行发生在循环后的 dave2d_overlay_end()。
                     */
                    d2_ok =
                        dave2d_overlay_draw_filled_rect(text_x - 1,
                                                        text_y - 1,
                                                        text_x + text_width,
                                                        text_y + 15,
                                                        0x0000U);
                    if (!d2_ok)
                    {
                        int32_t d2_error = dave2d_overlay_get_last_error();
                        (void) d2_error;
                        handle_error(FSP_ERR_INTERNAL,
                                     "** DAVE 2D DRAW LABEL BACKGROUND FAILED **\r\n");
                    }

                }

                /*
                 * 保持可显示旧版的提交边界：
                 * 先一次性执行并等待本帧的检测框和标签背景命令。
                 */
                d2_ok = dave2d_overlay_end();
                if (!d2_ok)
                {
                    int32_t d2_error = dave2d_overlay_get_last_error();
                    (void) d2_error;
                    handle_error(FSP_ERR_INTERNAL,
                                     "** DAVE 2D END FAILED **\r\n");
                }

                /*
                 * 文字仍由 D/AVE 2D 绘制，但每个标签使用一个独立的小命令批次。
                 *
                 * 这样既保持旧版“框和黑底先提交、文字后绘制”的稳定顺序，
                 * 又避免把一帧中所有字符的贴图命令堆积在同一个 render buffer 中。
                 */
                for (int index = 0; index < detection_count; index++)
                {
                    yolo_detection_t const * p_detection = &detections[index];
                    uint16_t color = g_yolo_class_colors[p_detection->class_id];
                    int x0 = 212 + (int) (p_detection->x1 * 600.0f / YOLO_INPUT_SIZE);
                    int y0 =       (int) (p_detection->y1 * 600.0f / YOLO_INPUT_SIZE);
                    char label[32];
                    unsigned int confidence =
                        (unsigned int) (p_detection->score * 100.0f + 0.5f);
                    int text_x = x0;
                    int text_y = (y0 >= 18) ? (y0 - 18) : (y0 + 2);

                    (void) snprintf(label,
                                    sizeof(label),
                                    "%s:%u",
                                    g_yolo_class_names[p_detection->class_id],
                                    confidence);

                    bool text_begin_ok =
                        dave2d_overlay_begin(p_draw_buffer,
                                             (int) CAMERA_IMAGE_WIDTH,
                                             (int) CAMERA_IMAGE_HEIGHT,
                                             DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0);

                    if (!text_begin_ok)
                    {
                        /*
                         * 文字叠加属于附加功能。即使本次命令批次无法建立，
                         * 也必须继续执行后面的 GLCDC 换帧，让摄像头原图正常显示。
                         */
                        int32_t d2_error = dave2d_overlay_get_last_error();
                        (void) d2_error;
                        break;
                    }

                    bool text_draw_ok =
                        dave2d_overlay_draw_text(text_x,
                                                 text_y,
                                                 label,
                                                 color,
                                                 DAVE2D_TEXT_SCALE);
                    //g_printf("text_draw_ok = %d\r\n",text_draw_ok);

                    int32_t text_draw_error = dave2d_overlay_get_last_error();
                    /*
                     * begin() 成功后必须调用 end() 关闭本次绘制会话。
                     * 即使文字命令录制失败，也要恢复 g_frame_active，避免下一帧一直报告 DEVICEBUSY。
                     */
                    bool text_end_ok = dave2d_overlay_end();

                    if ((!text_draw_ok) || (!text_end_ok))
                    {
                        int32_t d2_error = text_draw_ok ?
                                           dave2d_overlay_get_last_error() :
                                           text_draw_error;
                        (void) d2_error;

                        /*
                         * 不调用 handle_error()，因为它最终会执行 BKPT 并阻止
                         * R_GLCDC_BufferChange()。本帧只放弃剩余文字，摄像头画面继续显示。
                         */
                        break;
                    }
                }
            }
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
    const int dst_size = YOLO_INPUT_SIZE;
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
