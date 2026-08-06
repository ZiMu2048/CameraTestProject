#ifndef IMAGE_UPLOAD_IMAGE_JPEG_ENCODER_H_
#define IMAGE_UPLOAD_IMAGE_JPEG_ENCODER_H_

#include "hal_data.h"
#include <stddef.h>
#include <stdint.h>

typedef struct st_image_jpeg_encode_cfg
{
    uint16_t source_width;
    uint16_t source_height;
    uint16_t source_stride_pixels;

    uint16_t crop_x;
    uint16_t crop_y;
    uint16_t crop_width;
    uint16_t crop_height;

    uint16_t output_width;
    uint16_t output_height;

    uint8_t quality;
} image_jpeg_encode_cfg_t;

/*
 * 将 RGB565 framebuffer 的指定区域缩小并编码成 JPEG。
 * p_source_rgb565: 源 RGB565 framebuffer。
 * p_rgb888_workspace:调用者提供的 RGB888 中间缓冲区。
 * p_jpeg_output:调用者提供的 JPEG 输出缓冲区。
 * p_jpeg_size:返回实际生成的 JPEG 字节数。
 */
fsp_err_t ImageJpeg_EncodeRgb565(
    const uint16_t * p_source_rgb565,
    const image_jpeg_encode_cfg_t * p_cfg,
    uint8_t * p_rgb888_workspace,
    size_t rgb888_workspace_size,
    uint8_t * p_jpeg_output,
    size_t jpeg_output_capacity,
    size_t * p_jpeg_size);

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
    size_t * p_jpeg_size);


/*
 * 标量参考版：将 RGB565 指定区域裁剪、缩放并转换为 RGB888。
 * 当前用于验证算法正确性，后续 Helium 版本必须与其逐字节比较。
 */
fsp_err_t ImageJpeg_ConvertRgb565ToRgb888Scalar(
    const uint16_t * p_source_rgb565,
    const image_jpeg_encode_cfg_t * p_cfg,
    uint8_t * p_destination_rgb888,
    size_t destination_size);

/*
 * 使用固定颜色图像验证标量 RGB565 到 RGB888 转换函数。
 */
fsp_err_t ImageJpeg_SelfTestScalar(void);

/*
 * 使用 Arm Helium MVE 完成 RGB565 裁剪、最近邻缩放和 RGB888 转换。
 */
fsp_err_t ImageJpeg_ConvertRgb565ToRgb888Helium(
    const uint16_t * p_source_rgb565,
    const image_jpeg_encode_cfg_t * p_cfg,
    uint8_t * p_destination_rgb888,
    size_t destination_size);

/*
 * 固定图案 JPEG 编码自检。
 */
fsp_err_t ImageJpeg_SelfTestHelium(void);

fsp_err_t ImageJpeg_SelfTestEncode(size_t * p_jpeg_size);

/**
 * @brief 获取最近一次成功编码并发布的 JPEG 数据。
 * @param[out] pp_jpeg_data 返回模块内部 JPEG 缓冲区的只读首地址。
 * @param[out] p_jpeg_size 返回有效 JPEG 数据长度，单位为字节。
 * @return 数据有效时返回 FSP_SUCCESS，否则返回对应错误码。
 * @note 返回的数据在下一次编码开始前保持有效；调用者只能读取，当前不支持并发调用。
 */
fsp_err_t ImageJpeg_GetEncodedData(
    const uint8_t ** pp_jpeg_data,
    size_t * p_jpeg_size);

#endif /* IMAGE_UPLOAD_IMAGE_JPEG_ENCODER_H_ */
/*
 * ======================== 本项目使用的 Arm Helium MVE intrinsic 说明 ========================
 *
 * uint16x8_t
 *     128 位无符号整数向量类型，包含 8 个相互独立的 uint16_t 通道。
 *
 * mve_pred16_t
 *     16 位元素操作使用的谓词类型，记录 8 个 uint16_t 通道中哪些通道有效。
 *
 * vld1q_u16(address)
 *     从连续内存中读取 8 个 uint16_t，并装入一个 128 位向量寄存器。
 *
 * vctp16q(count)
 *     根据剩余元素数量生成 16 位通道谓词，用于处理不足 8 个像素的尾部数据。
 *
 * vldrhq_gather_shifted_offset_z_u16(base, offsets, predicate)
 *     以 base 为基地址，按 offsets 中的 uint16_t 元素索引聚集读取 RGB565 数据。
 *     shifted offset 会将索引自动乘以 2，谓词关闭的通道返回 0，避免尾部越界读取。
 *
 * vdupq_n_u16(value)
 *     将同一个 uint16_t 常数复制到 8 个向量通道，常用于生成位掩码。
 *
 * vshrq_n_u16(vector, bits)
 *     将 8 个 uint16_t 通道同时逻辑右移固定的 bits 位。
 *
 * vshlq_n_u16(vector, bits)
 *     将 8 个 uint16_t 通道同时左移固定的 bits 位。
 *
 * vandq_u16(a, b)
 *     对 a 和 b 的 8 个对应通道分别执行按位与，用于提取 RGB565 颜色字段。
 *
 * vorrq_u16(a, b)
 *     对 a 和 b 的 8 个对应通道分别执行按位或，用于完成颜色位复制扩展。
 *
 * vstrbq_scatter_offset_p_u16(base, offsets, values, predicate)
 *     将 values 各通道的低 8 位按照字节偏移 offsets 分散写入内存。
 *     只有谓词启用的通道会执行写入，用于生成 RGBRGB 交错排列并保护尾部边界。
 *
 * 详细定义以编译器附带的 arm_mve.h 和 Arm MVE Intrinsics Reference 为准。
 * ===========================================================================================
 */
