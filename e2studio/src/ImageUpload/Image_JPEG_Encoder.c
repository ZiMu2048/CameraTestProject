#include "Image_JPEG_Encoder.h"
#include <stdbool.h>
#include <string.h>

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ThirdParty/stb_image_write.h"

/*=================================引入Helium指令集技术==================================*/
#if !defined(__ARM_FEATURE_MVE) || ((__ARM_FEATURE_MVE & 1) == 0)
#error "Arm Helium MVE is not enabled for Image_JPEG_Encoder.c"
#endif

#include <arm_mve.h>
#define IMAGE_MVE_PIXELS_PER_VECTOR    (8U)//Helium 每次处理的向量像素数
/*======================================================================================*/

/*==================本项目使用240*240_RGB888图像编码,64KiB为JPEG最大数出===================*/
#define IMAGE_UPLOAD_WIDTH             (240U)
#define IMAGE_UPLOAD_HEIGHT            (240U)

#define IMAGE_RGB888_WORKSPACE_SIZE    \
    (IMAGE_UPLOAD_WIDTH * IMAGE_UPLOAD_HEIGHT * 3U)

#define IMAGE_JPEG_MAX_SIZE            (64U * 1024U)
/*=======================================================================================*/

/*=================================将数组放置在片外SDRAM中=================================*/
static uint8_t g_upload_rgb888[IMAGE_RGB888_WORKSPACE_SIZE]
    BSP_ALIGN_VARIABLE(64)
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");

static uint8_t g_upload_jpeg[IMAGE_JPEG_MAX_SIZE]
    BSP_ALIGN_VARIABLE(64)
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
/*
 * 最近一次成功发布的 JPEG 有效长度。
 * 零表示当前没有可供外部读取的完整 JPEG。
 */
static size_t g_upload_jpeg_size = 0U;
/*========================================================================================*/

/*===================================JPEG写入上下文结构体===================================*/
typedef struct st_image_jpeg_write_context
{
    uint8_t * p_buffer;      /* JPEG 输出缓冲区。 */
    size_t capacity;         /* 缓冲区总容量。 */
    size_t size;             /* 当前已写入字节数。 */
    bool overflow;           /* 容量不足标志。 */
} image_jpeg_write_context_t;
/*========================================================================================*/

#define IMAGE_JPEG_RGB_COMPONENTS    (3)
#define IMAGE_JPEG_QUALITY_MIN       (1U)
#define IMAGE_JPEG_QUALITY_MAX       (100U)

static void image_jpeg_write_callback(
    void * p_context,
    void * p_data,
    int data_size);

/**
 * @brief 使用标量 C 代码完成 RGB565 裁剪、最近邻缩放和 RGB888 转换。
 * @param[in] p_source_rgb565 RGB565 源帧缓冲区首地址。
 * @param[in] p_cfg 源尺寸、行步长、裁剪区域和输出尺寸配置。
 * @param[out] p_destination_rgb888 RGB888 输出缓冲区首地址。
 * @param[in] destination_size RGB888 输出缓冲区容量，单位为字节。
 * @return 转换成功时返回 FSP_SUCCESS，参数、尺寸或缓冲区无效时返回对应错误码。
 * @note 该实现是 Helium 版本的正确性参考，不可在中断中调用。
 */
fsp_err_t ImageJpeg_ConvertRgb565ToRgb888Scalar(
    const uint16_t * p_source_rgb565,
    const image_jpeg_encode_cfg_t * p_cfg,
    uint8_t * p_destination_rgb888,
    size_t destination_size)
{
    size_t required_size;

    if(NULL == p_source_rgb565 ||
       NULL ==p_cfg ||
       NULL == p_destination_rgb888)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if ((0U == p_cfg->source_width) ||
        (0U == p_cfg->source_height) ||
        (0U == p_cfg->source_stride_pixels) ||
        (0U == p_cfg->crop_width) ||
        (0U == p_cfg->crop_height) ||
        (0U == p_cfg->output_width) ||
        (0U == p_cfg->output_height))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (p_cfg->source_stride_pixels < p_cfg->source_width)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if ((((uint32_t) p_cfg->crop_x + p_cfg->crop_width) >
         p_cfg->source_width) ||
        (((uint32_t) p_cfg->crop_y + p_cfg->crop_height) >
         p_cfg->source_height))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if ((size_t) p_cfg->output_width >
        (SIZE_MAX / (size_t) p_cfg->output_height))
    {
        return FSP_ERR_INVALID_SIZE;
    }

    required_size =
        (size_t) p_cfg->output_width *
        (size_t) p_cfg->output_height;

    if (required_size > (SIZE_MAX / 3U))
    {
        return FSP_ERR_INVALID_SIZE;
    }

    required_size *= 3U;

    if (destination_size < required_size)
    {
        return FSP_ERR_INVALID_SIZE;
    }

    for (uint32_t destination_y = 0U;
         destination_y < p_cfg->output_height;
         destination_y++)
    {
        uint32_t source_y =
            (uint32_t) p_cfg->crop_y +
            ((destination_y * p_cfg->crop_height) /
             p_cfg->output_height);

        for (uint32_t destination_x = 0U;
             destination_x < p_cfg->output_width;
             destination_x++)
        {
            uint32_t source_x =
                (uint32_t) p_cfg->crop_x +
                ((destination_x * p_cfg->crop_width) /
                 p_cfg->output_width);

            size_t source_index =
                ((size_t) source_y *
                 p_cfg->source_stride_pixels) +
                source_x;

            size_t destination_index =
                (((size_t) destination_y *
                  p_cfg->output_width) +
                 destination_x) * 3U;

            uint16_t rgb565 = p_source_rgb565[source_index];

            uint8_t red_5 =
                (uint8_t) ((rgb565 >> 11U) & 0x1FU);

            uint8_t green_6 =
                (uint8_t) ((rgb565 >> 5U) & 0x3FU);

            uint8_t blue_5 =
                (uint8_t) (rgb565 & 0x1FU);

            /*
             * 位复制扩展比单纯左移更接近完整的 0～255 映射。
             */
            p_destination_rgb888[destination_index + 0U] =
                (uint8_t) ((red_5 << 3U) | (red_5 >> 2U));

            p_destination_rgb888[destination_index + 1U] =
                (uint8_t) ((green_6 << 2U) | (green_6 >> 4U));

            p_destination_rgb888[destination_index + 2U] =
                (uint8_t) ((blue_5 << 3U) | (blue_5 >> 2U));
        }
    }

    return FSP_SUCCESS;
}

/**
 * @brief 使用固定红、绿、蓝、白图案验证标量颜色转换和最近邻缩放结果。
 * @param 无。
 * @return 实际输出与固定参考数据一致时返回 FSP_SUCCESS，否则返回错误码。
 * @note 【测试代码，JPEG 链路稳定后删除】仅在系统启动阶段调用一次。
 */
fsp_err_t ImageJpeg_SelfTestScalar(void)
{
    fsp_err_t err;

    static const uint16_t source_rgb565[4] =
    {
        0xF800U, 0x07E0U,
        0x001FU, 0xFFFFU
    };// 预期转换输入数据
    static const uint8_t expected_rgb888[48] =
    {
        /* 第 0 行：红、红、绿、绿 */
        0xFFU, 0x00U, 0x00U,
        0xFFU, 0x00U, 0x00U,
        0x00U, 0xFFU, 0x00U,
        0x00U, 0xFFU, 0x00U,

        /* 第 1 行：红、红、绿、绿 */
        0xFFU, 0x00U, 0x00U,
        0xFFU, 0x00U, 0x00U,
        0x00U, 0xFFU, 0x00U,
        0x00U, 0xFFU, 0x00U,

        /* 第 2 行：蓝、蓝、白、白 */
        0x00U, 0x00U, 0xFFU,
        0x00U, 0x00U, 0xFFU,
        0xFFU, 0xFFU, 0xFFU,
        0xFFU, 0xFFU, 0xFFU,

        /* 第 3 行：蓝、蓝、白、白 */
        0x00U, 0x00U, 0xFFU,
        0x00U, 0x00U, 0xFFU,
        0xFFU, 0xFFU, 0xFFU,
        0xFFU, 0xFFU, 0xFFU
    };// 预期转换结果
    uint8_t output_rgb888[48] = {0};// 实际转换结果
    const image_jpeg_encode_cfg_t cfg =
    {
        .source_width         = 2U,//源高
        .source_height        = 2U,//源宽
        .source_stride_pixels = 2U,//源步幅

        .crop_x               = 0U,//裁剪起点X
        .crop_y               = 0U,//裁剪起点Y
        .crop_width           = 2U,//裁剪宽度
        .crop_height          = 2U,//裁剪高度

        .output_width         = 4U,//输出宽度
        .output_height        = 4U,//输出高度

        .quality              = 60U// JPEG质量
    };// 测试配置：将 2x2 RGB565 图像缩放为 4x4 RGB888 图像

    err = ImageJpeg_ConvertRgb565ToRgb888Scalar(
        source_rgb565,
        &cfg,
        output_rgb888,
        sizeof(output_rgb888));

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    for (size_t index = 0U;
         index < sizeof(expected_rgb888);
         index++)
    {
        if (output_rgb888[index] != expected_rgb888[index])
        {
            return FSP_ERR_ASSERTION;
        }
    }

    return FSP_SUCCESS;
}

/**
 * @brief 使用 Arm Helium MVE 完成 RGB565 裁剪、最近邻缩放和 RGB888 转换。
 * @param[in] p_source_rgb565 RGB565 源帧缓冲区首地址。
 * @param[in] p_cfg 源尺寸、行步长、裁剪区域和输出尺寸配置。
 * @param[out] p_destination_rgb888 RGB888 输出缓冲区首地址。
 * @param[in] destination_size RGB888 输出缓冲区容量，单位为字节。
 * @return 转换成功时返回 FSP_SUCCESS，参数、尺寸或缓冲区无效时返回对应错误码。
 * @note 每个 MVE 向量最多并行处理八个 RGB565 像素，不可在中断中调用。
 */
fsp_err_t ImageJpeg_ConvertRgb565ToRgb888Helium(
    const uint16_t * p_source_rgb565,
    const image_jpeg_encode_cfg_t * p_cfg,
    uint8_t * p_destination_rgb888,
    size_t destination_size)
{
    /* RGB888 每像素占 3 字节，此表给出连续八个像素相同颜色通道的字节偏移 */
    static const uint16_t rgb888_byte_offsets[IMAGE_MVE_PIXELS_PER_VECTOR]
        BSP_ALIGN_VARIABLE(16) =
    {
        0U, 3U, 6U, 9U, 12U, 15U, 18U, 21U
    };

    size_t required_size;

    /* 检查所有必需指针，防止访问空地址 */
    //***** 检查必需指针，防止访问空地址。 *****
    if ((NULL == p_source_rgb565) ||
        (NULL == p_cfg) ||
        (NULL == p_destination_rgb888))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* 检查尺寸和步长，同时避免后续缩放计算除以零 */
    if ((0U == p_cfg->source_width) ||
        (0U == p_cfg->source_height) ||
        (0U == p_cfg->source_stride_pixels) ||
        (0U == p_cfg->crop_width) ||
        (0U == p_cfg->crop_height) ||
        (0U == p_cfg->output_width) ||
        (0U == p_cfg->output_height))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* 每行内存跨度不能小于一行有效像素数量 */
    if (p_cfg->source_stride_pixels < p_cfg->source_width)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* 确保裁剪区域完全位于源图像内部 */
    if ((((uint32_t) p_cfg->crop_x + p_cfg->crop_width) > p_cfg->source_width) ||
        (((uint32_t) p_cfg->crop_y + p_cfg->crop_height) > p_cfg->source_height))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* 在乘法前检查 width × height 是否会超出 size_t 范围
     * required_size = output_width × output_height × 3
     * if:a > SIZE_MAX / b; else: a * b > SIZE_MAX >>溢出
    */
    if ((size_t) p_cfg->output_width >
        (SIZE_MAX / (size_t) p_cfg->output_height))
    {
        return FSP_ERR_INVALID_SIZE;
    }

    required_size = (size_t) p_cfg->output_width * (size_t) p_cfg->output_height;

    /* RGB888 每像素占 3 字节，检查乘以 3 是否溢出 */
    if (required_size > (SIZE_MAX / 3U))
    {
        return FSP_ERR_INVALID_SIZE;
    }

    required_size *= 3U;

    /* 确保输出缓冲区可以容纳完整 RGB888 图像 */
    if (destination_size < required_size)
    {
        return FSP_ERR_INVALID_SIZE;
    }

    /* 将八个 RGB888 字节偏移一次装入 128 位 MVE 向量寄存器 */
    const uint16x8_t output_offset_vector = vld1q_u16(rgb888_byte_offsets);

     for (uint32_t destination_y = 0U;
         destination_y < p_cfg->output_height;
         destination_y++)
    {
        /* 使用最近邻公式计算当前目标行对应的源图像行 */
        uint32_t source_y =
            (uint32_t) p_cfg->crop_y +
            ((destination_y * p_cfg->crop_height) /
             p_cfg->output_height);

        /* 定位到裁剪区域内当前源图像行的第一个 RGB565 像素 */
        const uint16_t * p_source_row =
            p_source_rgb565 +
            ((size_t) source_y *
             p_cfg->source_stride_pixels) +
            p_cfg->crop_x;

        for (uint32_t destination_x = 0U;
             destination_x < p_cfg->output_width;
             destination_x += IMAGE_MVE_PIXELS_PER_VECTOR)
        {
            /* 保存本轮最多八个目标像素对应的源像素横向索引 */
            uint16_t source_offsets[IMAGE_MVE_PIXELS_PER_VECTOR]
                BSP_ALIGN_VARIABLE(16) = {0U};

            /* 最后一轮不足八个像素时，只启用实际剩余的向量通道 */
            uint32_t active_lanes =
                (uint32_t) p_cfg->output_width -
                destination_x;

            if (active_lanes > IMAGE_MVE_PIXELS_PER_VECTOR)
            {
                active_lanes = IMAGE_MVE_PIXELS_PER_VECTOR;
            }

            /* 使用最近邻公式计算每个目标像素对应的源像素索引 */
            for (uint32_t lane = 0U;
                 lane < active_lanes;
                 lane++)
            {
                source_offsets[lane] =
                    (uint16_t)
                    ((((destination_x + lane) *
                       p_cfg->crop_width)) /
                     p_cfg->output_width);
            }

            /* 生成 16 位通道谓词，关闭尾部无效通道，防止越界读写 */
            mve_pred16_t predicate =
                vctp16q(active_lanes);

            /* 将八个源像素索引装入 MVE 向量寄存器 */
            uint16x8_t source_offset_vector =
                vld1q_u16(source_offsets);

            /*
             * Gather load 会从八个不同的源像素位置读取 RGB565
             * shifted offset 会自动把 uint16_t 索引乘以 2
             */
            uint16x8_t rgb565_vector =
                vldrhq_gather_shifted_offset_z_u16(
                    p_source_row,
                    source_offset_vector,
                    predicate);

            /* 提取 RGB565 中的 R5、G6、B5 分量 */
            uint16x8_t red_5_vector =
                vandq_u16(
                    vshrq_n_u16(rgb565_vector, 11),
                    vdupq_n_u16(0x1FU));

            uint16x8_t green_6_vector =
                vandq_u16(
                    vshrq_n_u16(rgb565_vector, 5),
                    vdupq_n_u16(0x3FU));

            uint16x8_t blue_5_vector =
                vandq_u16(
                    rgb565_vector,
                    vdupq_n_u16(0x1FU));

            /* R5 -> R8 */
            uint16x8_t red_8_vector =
                vorrq_u16(
                    vshlq_n_u16(red_5_vector, 3),
                    vshrq_n_u16(red_5_vector, 2));

            /* G6 -> G8 */
            uint16x8_t green_8_vector =
                vorrq_u16(
                    vshlq_n_u16(green_6_vector, 2),
                    vshrq_n_u16(green_6_vector, 4));

            /* B5 -> B8 */
            uint16x8_t blue_8_vector =
                vorrq_u16(
                    vshlq_n_u16(blue_5_vector, 3),
                    vshrq_n_u16(blue_5_vector, 2));

            /* 定位到本轮八个目标像素在 RGB888 输出缓冲区中的起点 */
            uint8_t * p_destination_block =
                p_destination_rgb888 +
                ((((size_t) destination_y *
                   p_cfg->output_width) +
                  destination_x) * 3U);

            /*
             * 分别写入 RGB888 中的 R、G、B 字节
             * scatter offset 的偏移单位是字节
             */
            vstrbq_scatter_offset_p_u16(
                p_destination_block + 0U,
                output_offset_vector,
                red_8_vector,
                predicate);

            /* 基地址加 1，分散写入八个绿色分量 */
            vstrbq_scatter_offset_p_u16(
                p_destination_block + 1U,
                output_offset_vector,
                green_8_vector,
                predicate);

            /* 基地址加 2，分散写入八个蓝色分量 */
            vstrbq_scatter_offset_p_u16(
                p_destination_block + 2U,
                output_offset_vector,
                blue_8_vector,
                predicate);
        }
    }

    return FSP_SUCCESS;
}

/**
 * @brief 使用相同输入逐字节比较标量版本与 Helium 版本的 RGB888 输出。
 * @param 无。
 * @return 两个实现的输出完全一致时返回 FSP_SUCCESS，否则返回错误码。
 * @note 【测试代码，JPEG 链路稳定后删除】仅在系统启动阶段调用一次。
 */
fsp_err_t ImageJpeg_SelfTestHelium(void)
{
    static const uint16_t source_rgb565[4] =
    {
        0xF800U, 0x07E0U,
        0x001FU, 0xFFFFU
    };

    const image_jpeg_encode_cfg_t cfg =
    {
        .source_width         = 2U,
        .source_height        = 2U,
        .source_stride_pixels = 2U,

        .crop_x               = 0U,
        .crop_y               = 0U,
        .crop_width           = 2U,
        .crop_height          = 2U,

        .output_width         = 4U,
        .output_height        = 4U,

        .quality              = 60U
    };

    uint8_t scalar_output[48] = {0U};
    uint8_t helium_output[48] = {0U};

    fsp_err_t err = ImageJpeg_ConvertRgb565ToRgb888Scalar(
        source_rgb565,
        &cfg,
        scalar_output,
        sizeof(scalar_output));

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = ImageJpeg_ConvertRgb565ToRgb888Helium(
        source_rgb565,
        &cfg,
        helium_output,
        sizeof(helium_output));

    if (FSP_SUCCESS != err)
    {
        return err;
    }

     for (size_t index = 0U;
         index < sizeof(scalar_output);
         index++)
    {
        if (scalar_output[index] != helium_output[index])
        {
            return FSP_ERR_ASSERTION;
        }
    }

    return FSP_SUCCESS;
}

/**
 * @brief 将 RGB565 指定区域转换为 RGB888，并编码到调用者提供的 JPEG 缓冲区。
 * @param[in] p_source_rgb565 RGB565 源帧缓冲区首地址。
 * @param[in] p_cfg 裁剪、输出尺寸和 JPEG 质量配置。
 * @param[out] p_rgb888_workspace RGB888 中间工作缓冲区。
 * @param[in] rgb888_workspace_size RGB888 工作缓冲区容量，单位为字节。
 * @param[out] p_jpeg_output JPEG 输出缓冲区。
 * @param[in] jpeg_output_capacity JPEG 输出缓冲区容量，单位为字节。
 * @param[out] p_jpeg_size 实际生成的 JPEG 字节数。
 * @return 编码成功时返回 FSP_SUCCESS，否则返回对应错误码。
 * @note 当前为阻塞式软件 JPEG 编码，不可在中断中调用。
 */
fsp_err_t ImageJpeg_EncodeRgb565(
    const uint16_t * p_source_rgb565,
    const image_jpeg_encode_cfg_t * p_cfg,
    uint8_t * p_rgb888_workspace,
    size_t rgb888_workspace_size,
    uint8_t * p_jpeg_output,
    size_t jpeg_output_capacity,
    size_t * p_jpeg_size)
{
    image_jpeg_write_context_t write_context;
    fsp_err_t err;
    int encode_result;

    /* 检查所有必需指针 */
    if ((NULL == p_source_rgb565) ||
        (NULL == p_cfg) ||
        (NULL == p_rgb888_workspace) ||
        (NULL == p_jpeg_output) ||
        (NULL == p_jpeg_size))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* 失败时默认返回零长度 */
    *p_jpeg_size = 0U;

    if(p_cfg->quality < IMAGE_JPEG_QUALITY_MIN ||
       p_cfg->quality > IMAGE_JPEG_QUALITY_MAX)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* 输出区非空 */
    if(0U == jpeg_output_capacity)
    {
        return FSP_ERR_INVALID_SIZE;
    }

    /* 生成 RGB888 中间图像 */
    err = ImageJpeg_ConvertRgb565ToRgb888Helium(
        p_source_rgb565,//源地址
        p_cfg,//JPEG配置
        p_rgb888_workspace,//RGB888缓冲区
        rgb888_workspace_size);
    if(FSP_SUCCESS != err)
    {
        return err;
    }

    /* 初始化 stb 多次回调共同使用的内存写入状态 */
    write_context.p_buffer = p_jpeg_output;
    write_context.capacity = jpeg_output_capacity;
    write_context.size     = 0U;
    write_context.overflow = false;

    /* 将 RGB888 编码为 JPEG，并通过回调写入内存 */
    encode_result = stbi_write_jpg_to_func(
        image_jpeg_write_callback,
        &write_context,
        (int) p_cfg->output_width,
        (int) p_cfg->output_height,
        IMAGE_JPEG_RGB_COMPONENTS,
        p_rgb888_workspace,
        (int) p_cfg->quality);

    /* stb 返回 0 表示 JPEG 编码失败 */
    if (0 == encode_result)
    {
        return FSP_ERR_INTERNAL;
    }

    /* 回调检测到容量不足时，禁止使用不完整 JPEG */
    if (write_context.overflow)
    {
        return FSP_ERR_INVALID_SIZE;
    }

    /* 完整 JPEG 至少需要 SOI 和 EOI 两组标记 */
    if (write_context.size < 4U)
    {
        return FSP_ERR_INTERNAL;
    }

    /* 检查 JPEG 开头的 SOI 标记 FF D8 */
    if ((0xFFU != p_jpeg_output[0]) ||
        (0xD8U != p_jpeg_output[1]))
    {
        return FSP_ERR_INTERNAL;
    }

    /* 检查 JPEG 结尾的 EOI 标记 FF D9 */
    if ((0xFFU != p_jpeg_output[write_context.size - 2U]) ||
        (0xD9U != p_jpeg_output[write_context.size - 1U]))
    {
        return FSP_ERR_INTERNAL;
    }

    /* 只有全部检查通过后才返回有效 JPEG 长度。 */
    *p_jpeg_size = write_context.size;

    return FSP_SUCCESS;
}

/**
 * @brief 将 RGB565 图像编码到模块内部缓冲区并发布只读 JPEG 数据。
 * @param[in] p_source_rgb565 RGB565 源帧缓冲区首地址。
 * @param[in] p_cfg 裁剪、缩放、输出尺寸和 JPEG 质量配置。
 * @param[out] p_jpeg_size 返回实际生成并发布的 JPEG 字节数。
 * @return 编码和格式检查成功时返回 FSP_SUCCESS，否则返回对应错误码。
 * @note 本函数为阻塞调用；发送完成前不得再次编码，当前不支持并发调用，也不可在中断中调用。
 */
fsp_err_t ImageJpeg_EncodeAndPublishRgb565(
    const uint16_t * p_source_rgb565,
    const image_jpeg_encode_cfg_t * p_cfg,
    size_t * p_jpeg_size)
{
    size_t encoded_size = 0U;
    fsp_err_t err;

    if ((NULL == p_source_rgb565) ||
        (NULL == p_cfg) ||
        (NULL == p_jpeg_size))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *p_jpeg_size       = 0U;
    g_upload_jpeg_size = 0U;

    err = ImageJpeg_EncodeRgb565(
        p_source_rgb565,
        p_cfg,
        g_upload_rgb888,
        sizeof(g_upload_rgb888),
        g_upload_jpeg,
        sizeof(g_upload_jpeg),
        &encoded_size);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    g_upload_jpeg_size = encoded_size;
    *p_jpeg_size       = encoded_size;
    return FSP_SUCCESS;
}

/**
 * @brief 接收 stb_image_write 分批产生的 JPEG 数据并顺序写入内存缓冲区。
 * @param[in,out] p_context JPEG 内存写入状态。
 * @param[in] p_data 本次产生的 JPEG 数据块。
 * @param[in] data_size 本次数据块长度，单位为字节。
 * @return 无。
 * @note 发生容量不足时只设置 overflow，不执行越界或部分写入。
 */
static void image_jpeg_write_callback(
    void * p_context,
    void * p_data,
    int data_size)
{
    image_jpeg_write_context_t * p_write_context;

    if((NULL == p_context) ||
       (NULL == p_data) ||
       (data_size <= 0))
    {
        return;
    }

    p_write_context = (image_jpeg_write_context_t *) p_context;
    /* 检查前面是否已经发生溢出 */
    if(p_write_context->overflow)
    {
        return;
    }

    size_t chunk_size = (size_t) data_size;
    /* 检查当前长度是否有效，并确认剩余空间足够 */
    if((p_write_context->size > p_write_context->capacity) || //写入容量大于缓冲区容量
       (chunk_size > (p_write_context->capacity - p_write_context->size))) //剩余空间不足
       {
        p_write_context->overflow = true;
        return;
       }

    /* 将本次 JPEG 数据追加到已经写入的数据后 */
    memcpy(p_write_context->p_buffer + p_write_context->size,
           p_data,
           chunk_size);

    /* 更新已写入数据的总长度 */
    p_write_context->size += chunk_size;
}

/**
 * @brief 将固定 RGB565 彩色图案编码为 240×240 JPEG，用于验证完整编码链路。
 * @param[out] p_jpeg_size 返回实际生成的 JPEG 字节数。
 * @return 编码和 JPEG 格式检查成功时返回 FSP_SUCCESS，否则返回对应错误码。
 * @note 【测试代码，真实摄像头 JPEG 上传验证完成后删除】
 */
fsp_err_t ImageJpeg_SelfTestEncode(size_t * p_jpeg_size)
{
    static const uint16_t source_rgb565[4] =
    {
        0xF800U, 0x07E0U,
        0x001FU, 0xFFFFU
    };

    const image_jpeg_encode_cfg_t cfg =
    {
        .source_width         = 2U,
        .source_height        = 2U,
        .source_stride_pixels = 2U,

        .crop_x               = 0U,
        .crop_y               = 0U,
        .crop_width           = 2U,
        .crop_height          = 2U,

        .output_width         = IMAGE_UPLOAD_WIDTH,
        .output_height        = IMAGE_UPLOAD_HEIGHT,

        .quality              = 60U
    };

    if (NULL == p_jpeg_size)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    //***** 使用模块内部 SDRAM 缓冲区执行完整 JPEG 编码并发布结果。*****
    return ImageJpeg_EncodeAndPublishRgb565(
        source_rgb565,
        &cfg,
        p_jpeg_size);
}

/**
 * @brief 获取最近一次成功编码并发布的 JPEG 数据。
 * @param[out] pp_jpeg_data 返回模块内部 JPEG 缓冲区的只读首地址。
 * @param[out] p_jpeg_size 返回有效 JPEG 数据长度，单位为字节。
 * @return 数据有效时返回 FSP_SUCCESS；参数为空时返回 FSP_ERR_INVALID_ARGUMENT；
 *         尚无成功编码结果时返回 FSP_ERR_NOT_INITIALIZED。
 * @note 返回的数据在下一次 JPEG 编码开始前保持有效；调用者不得修改，
 *       当前实现不支持编码与读取并发进行，也不可在中断上下文中调用。
 */
fsp_err_t ImageJpeg_GetEncodedData(
    const uint8_t ** pp_jpeg_data,
    size_t * p_jpeg_size)
{
    if ((NULL == pp_jpeg_data) ||
        (NULL == p_jpeg_size))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *pp_jpeg_data = NULL;
    *p_jpeg_size  = 0U;

    /*
     * 长度为零表示尚未成功编码，或者新一轮编码正在进行。
     */
    if (0U == g_upload_jpeg_size)
    {
        return FSP_ERR_NOT_INITIALIZED;
    }

    *pp_jpeg_data = g_upload_jpeg;
    *p_jpeg_size  = g_upload_jpeg_size;

    return FSP_SUCCESS;
}
