#ifndef DAVE2D_OVERLAY_H
#define DAVE2D_OVERLAY_H

#include "hal_data.h"
#include <stdbool.h>
#include <stdint.h>

/* 初始化 D/AVE 2D 设备和命令缓冲区，整个应用生命周期只需调用一次。 */
bool dave2d_overlay_init(void);

/* 开始一帧批量绘制，绑定目标 RGB565 framebuffer。 */
bool dave2d_overlay_begin(void * framebuffer,
                          int width,
                          int height,
                          int pitch_pixels);

/* 向当前帧命令缓冲区追加一个矩形框，不立即提交硬件执行。 */
bool dave2d_overlay_draw_rect(int x0,
                              int y0,
                              int x1,
                              int y1,
                              uint16_t rgb565,
                              int line_width);

/* 提交当前帧全部命令并等待 D/AVE 2D 硬件完成。 */
bool dave2d_overlay_end(void);

/* 获取最近一次适配层或 D/AVE 2D API 返回的错误码。 */
int32_t dave2d_overlay_get_last_error(void);

/* 向当前帧命令缓冲区追加一个实心矩形，不立即提交硬件执行。 */
bool dave2d_overlay_draw_filled_rect(int x0,
                                     int y0,
                                     int x1,
                                     int y1,
                                     uint16_t rgb565);

#endif // DAVE2D_OVERLAY_H
