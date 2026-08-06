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
#include "ImageUpload/Image_JPEG_Encoder.h"

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

#define IMAGE_TCP_PROTOCOL_VERSION       (1U)
#define IMAGE_TCP_HEADER_SIZE            (24U)
#define IMAGE_TCP_JPEG_CHUNK_SIZE        (1024U)
#define IMAGE_TCP_SEND_TIMEOUT_MS        (5000U)

#define IMAGE_UPLOAD_SOURCE_WIDTH         (1024U)
#define IMAGE_UPLOAD_SOURCE_HEIGHT        (600U)
#define IMAGE_UPLOAD_SOURCE_STRIDE        (1024U)
#define IMAGE_UPLOAD_CROP_X               (212U)
#define IMAGE_UPLOAD_CROP_Y               (0U)
#define IMAGE_UPLOAD_CROP_WIDTH           (600U)
#define IMAGE_UPLOAD_CROP_HEIGHT          (600U)
#define IMAGE_UPLOAD_OUTPUT_WIDTH         (240U)
#define IMAGE_UPLOAD_OUTPUT_HEIGHT        (240U)
#define IMAGE_UPLOAD_JPEG_QUALITY         (60U)
#define IMAGE_UPLOAD_CLEAR_FRAMES         (10U)

static uint8_t g_da16200_tcp_cid = 0xFFU;


/* External variables */
extern sensor_reg_t live_camera;
extern const camera_config_t camera_profiles[RES_MAX];

extern const vin_extended_cfg_t g_vin_cfg_extend;

extern volatile uint8_t g_vsync_flag;

/* Global variables */
uint8_t * volatile gp_next_buffer;
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
static fsp_err_t da16200_send_published_jpeg(uint8_t cid,
                                             uint16_t width,
                                             uint16_t height,
                                             uint16_t confidence_milli);
static fsp_err_t da16200_encode_and_send_camera_frame(uint8_t cid,
                                                       const uint16_t * p_frame,
                                                       uint16_t confidence_milli);

/**
 * @brief 确保 DA16200 工作在 Station 模式。
 * @param 无。
 * @return 成功时返回 FSP_SUCCESS，AT 通信、复位或模式校验失败时返回对应错误码。
 * @note 该函数包含阻塞式 AT 命令和软件延时，只能在普通执行上下文中调用。
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

/**
 * @brief 探测 DA16200 AT 协议、固件版本、工作模式和 Station 联网状态。
 * @param 无。
 * @return 全部命令与状态检查通过时返回 FSP_SUCCESS，否则返回首次失败的错误码。
 * @note 该函数用于系统启动阶段，不可在中断中调用。
 */
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

/**
 * @brief 连接本地 Wi-Fi，并确认详细状态中已经出现 wpa_state=COMPLETED。
 * @param 无。
 * @return 联网并验证成功时返回 FSP_SUCCESS，否则返回连接、查询或状态断言错误。
 * @note Wi-Fi 凭据来自本文件的本地配置宏，日志不得输出真实密码。
 */
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

/**
 * @brief 使用已配置的电脑 IP 和端口建立 DA16200 TCP 客户端连接。
 * @param 无。
 * @return TCP 连接成功并取得 CID 时返回 FSP_SUCCESS，否则返回驱动错误码。
 * @note 成功取得的 CID 保存到 g_da16200_tcp_cid，供后续发送函数使用。
 */
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

/**
 * @brief 按网络大端字节序写入一个 16 位无符号整数。
 * @param[out] p_destination 两字节输出缓冲区首地址。
 * @param[in] value 需要序列化的 16 位数值。
 * @return 无。
 * @note 调用者必须保证输出缓冲区至少有两个可写字节；本函数不可传入空指针。
 */
static void image_tcp_store_u16_be(uint8_t * p_destination, uint16_t value)
{
    p_destination[0] = (uint8_t) (value >> 8U);
    p_destination[1] = (uint8_t) value;
}

/**
 * @brief 按网络大端字节序写入一个 32 位无符号整数。
 * @param[out] p_destination 四字节输出缓冲区首地址。
 * @param[in] value 需要序列化的 32 位数值。
 * @return 无。
 * @note 调用者必须保证输出缓冲区至少有四个可写字节；本函数不可传入空指针。
 */
static void image_tcp_store_u32_be(uint8_t * p_destination, uint32_t value)
{
    p_destination[0] = (uint8_t) (value >> 24U);
    p_destination[1] = (uint8_t) (value >> 16U);
    p_destination[2] = (uint8_t) (value >> 8U);
    p_destination[3] = (uint8_t) value;
}

/**
 * @brief 将最近一次发布的 JPEG 封装为图像帧并通过 DA16200 分块发送。
 * @param[in] cid 已建立的 DA16200 TCP Client 会话编号。
 * @param[in] width JPEG 图像宽度，单位为像素。
 * @param[in] height JPEG 图像高度，单位为像素。
 * @param[in] confidence_milli 检测置信度千分值，例如 0.735 对应 735。
 * @return 帧头和全部 JPEG 数据块发送成功时返回 FSP_SUCCESS，否则返回对应错误码。
 * @note 本函数为阻塞调用，只能在普通执行上下文调用；发送结束前不得启动下一次 JPEG 编码。
 */
static fsp_err_t da16200_send_published_jpeg(uint8_t cid,
                                             uint16_t width,
                                             uint16_t height,
                                             uint16_t confidence_milli)
{
    static uint32_t frame_id = 0U;
    uint8_t header[IMAGE_TCP_HEADER_SIZE] = {0};
    const uint8_t * p_jpeg_data = NULL;
    size_t jpeg_size = 0U;
    size_t offset = 0U;
    fsp_err_t err;

    err = ImageJpeg_GetEncodedData(&p_jpeg_data, &jpeg_size);
    if (FSP_SUCCESS != err)
    {
        g_printf("Image upload: encoded JPEG unavailable, err=%d\r\n",
                 (int) err);
        return err;
    }

    if ((jpeg_size < 4U) || (jpeg_size > UINT32_MAX))
    {
        return FSP_ERR_INVALID_SIZE;
    }

    frame_id++;

    header[0] = (uint8_t) 'R';
    header[1] = (uint8_t) 'J';
    header[2] = (uint8_t) 'P';
    header[3] = (uint8_t) 'G';
    header[4] = IMAGE_TCP_PROTOCOL_VERSION;
    header[5] = IMAGE_TCP_HEADER_SIZE;
    image_tcp_store_u16_be(&header[6], 0U);
    image_tcp_store_u32_be(&header[8], frame_id);
    image_tcp_store_u16_be(&header[12], width);
    image_tcp_store_u16_be(&header[14], height);
    image_tcp_store_u32_be(&header[16], (uint32_t) jpeg_size);
    image_tcp_store_u16_be(&header[20], confidence_milli);
    image_tcp_store_u16_be(&header[22], 0U);

    err = DA16200_TcpClientSendBinaryChunk(
        cid,
        header,
        (uint16_t) sizeof(header),
        IMAGE_TCP_SEND_TIMEOUT_MS);
    if (FSP_SUCCESS != err)
    {
        g_printf("Image upload: frame header send failed, err=%d\r\n",
                 (int) err);
        return err;
    }

    while (offset < jpeg_size)
    {
        size_t const remaining = jpeg_size - offset;
        uint16_t const chunk_size =
            (uint16_t) ((remaining > IMAGE_TCP_JPEG_CHUNK_SIZE) ?
                        IMAGE_TCP_JPEG_CHUNK_SIZE : remaining);

        err = DA16200_TcpClientSendBinaryChunk(
            cid,
            &p_jpeg_data[offset],
            chunk_size,
            IMAGE_TCP_SEND_TIMEOUT_MS);
        if (FSP_SUCCESS != err)
        {
            g_printf("Image upload: JPEG chunk failed at offset=%lu, err=%d\r\n",
                     (unsigned long) offset,
                     (int) err);
            return err;
        }

        offset += chunk_size;
    }

    g_printf("Image upload: frame=%lu, JPEG=%lu bytes sent\r\n",
             (unsigned long) frame_id,
             (unsigned long) jpeg_size);
    return FSP_SUCCESS;
}

/**
 * @brief 将一帧稳定的 1024×600 RGB565 摄像头图像编码并发送到电脑。
 * @param[in] cid 已建立的 DA16200 TCP Client 会话编号。
 * @param[in] p_frame 稳定且不会被 VIN 覆盖的 RGB565 帧缓冲区首地址。
 * @return JPEG 编码和全部 TCP 分块发送成功时返回 FSP_SUCCESS，否则返回对应错误码。
 * @note 当前固定执行中央 600×600 裁剪并缩放为 240×240；本函数为阻塞调用且不支持并发。
 */
/**
 * @brief Encode one stable RGB565 camera frame and send it as a JPEG error record.
 * @param[in] cid Connected DA16200 TCP client session ID.
 * @param[in] p_frame Stable 1024x600 RGB565 frame buffer.
 * @param[in] confidence_milli Detection confidence multiplied by 1000.
 * @return FSP_SUCCESS when JPEG encoding and all TCP chunks succeed, otherwise an error code.
 * @note This is a blocking function and must not be called from interrupt context.
 */
static fsp_err_t da16200_encode_and_send_camera_frame(uint8_t cid,
                                                       const uint16_t * p_frame,
                                                       uint16_t confidence_milli)
{
    image_jpeg_encode_cfg_t const cfg =
    {
        .source_width         = IMAGE_UPLOAD_SOURCE_WIDTH,
        .source_height        = IMAGE_UPLOAD_SOURCE_HEIGHT,
        .source_stride_pixels = IMAGE_UPLOAD_SOURCE_STRIDE,
        .crop_x               = IMAGE_UPLOAD_CROP_X,
        .crop_y               = IMAGE_UPLOAD_CROP_Y,
        .crop_width           = IMAGE_UPLOAD_CROP_WIDTH,
        .crop_height          = IMAGE_UPLOAD_CROP_HEIGHT,
        .output_width         = IMAGE_UPLOAD_OUTPUT_WIDTH,
        .output_height        = IMAGE_UPLOAD_OUTPUT_HEIGHT,
        .quality              = IMAGE_UPLOAD_JPEG_QUALITY
    };
    size_t jpeg_size = 0U;
    fsp_err_t err;

    if (NULL == p_frame)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    err = ImageJpeg_EncodeAndPublishRgb565(p_frame, &cfg, &jpeg_size);
    if (FSP_SUCCESS != err)
    {
        g_printf("Image upload: camera JPEG encode failed, err=%d\r\n",
                 (int) err);
        return err;
    }

    g_printf("Image upload: camera JPEG encoded, size=%lu bytes\r\n",
             (unsigned long) jpeg_size);

    return da16200_send_published_jpeg(
        cid,
        IMAGE_UPLOAD_OUTPUT_WIDTH,
        IMAGE_UPLOAD_OUTPUT_HEIGHT,
        confidence_milli);
}

/**
 * @brief 初始化终端、DA16200、摄像头、VIN、显示和 NPU，并运行实时采集与推理主循环。
 * @param 无。
 * @return 无。
 * @note 当前通信流程包含阻塞操作，该函数是应用主入口，不可在中断上下文中调用。
 */
void mipi_csi_ep_entry(void)
{
    //fsp_pack_version_t  version = {RESET_VALUE};
    fsp_err_t           err     = FSP_SUCCESS;
#if (DISPLAY_OUTPUT == 1U)
    /* GLCDC starts with fb_background[0]. CPU always renders the other buffer. */
    uint8_t draw_buffer_index = 1U;
    bool error_event_latched = false;
    uint8_t clean_frame_count = 0U;
#endif

    /* Initialize the terminal */
    TERM_INIT();
    /*===========图像裁剪自检==================*/
    err = ImageJpeg_SelfTestScalar();
    if (FSP_SUCCESS == err)
    {
        g_printf("Image JPEG: scalar conversion self-test passed\r\n");
    }
    else
    {
        g_printf("Image JPEG: scalar conversion self-test failed, err=%d\r\n",
                (int) err);
    }

    err = ImageJpeg_SelfTestHelium();
    if (FSP_SUCCESS == err)
    {
        g_printf("Image JPEG: Helium conversion self-test passed\r\n");
    }
    else
    {
        g_printf("Image JPEG: Helium conversion self-test failed, err=%d\r\n",
                (int) err);
    }

    size_t jpeg_test_size = 0U;
    err = ImageJpeg_SelfTestEncode(&jpeg_test_size);

    if (FSP_SUCCESS == err)
    {
        g_printf(
            "Image JPEG: encode self-test passed, size=%lu bytes\r\n",
            (unsigned long) jpeg_test_size);
    }
    else
    {
        g_printf(
            "Image JPEG: encode self-test failed, err=%d\r\n",
            (int) err);
    }
    /*=======================================*/
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

#if BSP_CFG_DCACHE_ENABLED
    /*
     * VIN is a DMA producer and does not update the Cortex-M85 D-Cache.
     * Clean the initialization writes, then invalidate all cached copies before
     * handing the three buffers to VIN.
     */
    SCB_CleanInvalidateDCache_by_Addr(
        (uint32_t *) vin_image_buffer_1,
        (int32_t) VIN_BYTES_PER_FRAME);
    SCB_CleanInvalidateDCache_by_Addr(
        (uint32_t *) vin_image_buffer_2,
        (int32_t) VIN_BYTES_PER_FRAME);
    SCB_CleanInvalidateDCache_by_Addr(
        (uint32_t *) vin_image_buffer_3,
        (int32_t) VIN_BYTES_PER_FRAME);
#endif

    
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
        uint8_t * p_completed_frame;

        /*垂直同步*/
        g_vsync_flag = RESET_FLAG;
        while(!g_vsync_flag);

        /*
         * Snapshot the ISR-published pointer once. The callback may publish a
         * newer completed buffer while this loop is running.
         */
        p_completed_frame = gp_next_buffer;

        /*把 VIN 最新帧复制到显示 framebuffer*/
        if (p_completed_frame != NULL)
        {
#if BSP_CFG_DCACHE_ENABLED
            /* Discard stale CPU cache lines before reading the DMA-written frame. */
            SCB_InvalidateDCache_by_Addr(
                p_completed_frame,
                (int32_t) VIN_BYTES_PER_FRAME);
#endif
            memcpy(p_draw_buffer, p_completed_frame, VIN_BYTES_PER_FRAME);

        }

        /*算子工作*/
        if (p_completed_frame != NULL)
        {
            int8_t * model_input = GetModelInputPtr_x();//获取模型输入缓冲区指针
            preprocess_frame_to_yolo(p_completed_frame, model_input);//输入图像伸缩预处理

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
            float max_confidence = 0.0f;
            for (int index = 0; index < detection_count; index++)
            {
                if (detections[index].score > max_confidence)
                {
                    max_confidence = detections[index].score;
                }
            }

            /*
             * Upload only the first frame of one continuous error event.
             * Ten consecutive clean inference frames re-arm the trigger.
             */
            if ((detection_count > 0) &&
                (max_confidence > MAXTRUSTTHRESHOLD))
            {
                clean_frame_count = 0U;

                if ((!error_event_latched) &&
                    (g_da16200_tcp_cid <= 7U))
                {
                    uint16_t confidence_milli =
                        (max_confidence >= 1.0f) ?
                        1000U :
                        (uint16_t) (max_confidence * 1000.0f + 0.5f);

                    /* Latch before the blocking transfer to avoid retry storms. */
                    error_event_latched = true;
                    err = da16200_encode_and_send_camera_frame(
                        g_da16200_tcp_cid,
                        (const uint16_t *) p_draw_buffer,
                        confidence_milli);
                    if (FSP_SUCCESS != err)
                    {
                        g_printf(
                            "Image upload: detection frame failed, confidence=%u, err=%d\r\n",
                            (unsigned int) confidence_milli,
                            (int) err);
                    }
                }
            }
            else
            {
                if (clean_frame_count < IMAGE_UPLOAD_CLEAR_FRAMES)
                {
                    clean_frame_count++;
                }

                if (clean_frame_count >= IMAGE_UPLOAD_CLEAR_FRAMES)
                {
                    error_event_latched = false;
                }
            }

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

/**
 * @brief 处理 VIN 捕获回调，并在一帧完成时更新下一帧缓冲区指针。
 * @param[in] p_args VIN 驱动传入的事件、状态与帧缓冲区信息。
 * @return 无。
 * @note 该函数运行在回调上下文中，只执行必要的状态读取和指针更新。
 */
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

/**
 * @brief 接收 MIPI CSI 外设事件，并忽略当前应用未使用的事件类型。
 * @param[in] p_args MIPI CSI 驱动传入的事件信息。
 * @return 无。
 * @note 该函数运行在回调上下文中，不执行日志、大块复制或阻塞操作。
 */
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

/**
 * @brief 按给定配置重新打开 VIN，并依次启动 VIN 捕获和摄像头数据流。
 * @param[in] p_cfg 需要应用的 VIN 捕获配置。
 * @return 全部启动步骤成功时返回 FSP_SUCCESS，否则返回对应驱动错误码。
 * @note 调用过程中会停止当前视频流并重新配置 VIN，不可在中断中调用。
 */
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

/**
 * @brief 根据目标宽高计算 VIN 缩放参数，并生成运行时配置副本。
 * @param[in] new_width 目标输出宽度，单位为像素。
 * @param[in] new_height 目标输出高度，单位为像素。
 * @return 配置计算成功时返回 FSP_SUCCESS，尺寸为零时返回 FSP_ERR_INVALID_ARGUMENT。
 * @note 该函数只更新运行时配置结构体，不直接启动 VIN 硬件。
 */
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

/**
 * @brief 处理不可恢复错误，打印信息、关闭已打开外设并进入错误陷阱。
 * @param[in] err 需要处理的 FSP 错误码。
 * @param[in] err_str 需要输出的错误说明字符串。
 * @return 无。
 * @note 当 err 为 FSP_SUCCESS 时不执行任何操作，否则该函数通常不会正常返回。
 */
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

/**
 * @brief 将 VIN 的 RGB565 中央裁剪区域缩放并转换为 YOLO 所需的 int8 RGB 输入。
 * @param[in] src VIN RGB565 帧缓冲区首地址。
 * @param[out] dst YOLO 输入张量缓冲区首地址。
 * @return 无。
 * @note 当前使用固定裁剪尺寸、目标尺寸和源行步长，调用者必须保证缓冲区有效。
 */
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
