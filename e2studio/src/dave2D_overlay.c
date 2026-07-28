#include "dave2D_overlay.h"
#include "dave_driver.h"
#include "dave_math.h"

#define DAVE2D_RENDER_BUFFER_INITIAL_SIZE    (256U)
#define DAVE2D_RENDER_BUFFER_STEP_SIZE       (128U)

#define TEST_RECT_X0                         (100)
#define TEST_RECT_Y0                         (100)
#define TEST_RECT_X1                         (400)
#define TEST_RECT_Y1                         (300)
#define TEST_RECT_LINE_WIDTH                 (3)

/* DAVE 2D 设备句柄 */
static d2_device       * gp_d2_device = NULL;
/* 存放画线命令 */
static d2_renderbuffer * gp_d2_render_buffer = NULL;
static int32_t g_d2_last_error = D2_OK;

/*
 * RGB565->0x00RRGGBB
 */
static d2_color rgb565_to_d2_color(uint16_t rgb565)
{
    uint32_t red_5   = (rgb565 >> 11) & 0x1FU;
    uint32_t green_6 = (rgb565 >> 5)  & 0x3FU;
    uint32_t blue_5  = rgb565 & 0x1FU;

    /*
     * 将 5/6 bit 色彩扩展到 8 bit。
     */
    uint32_t red_8   = (red_5 << 3) | (red_5 >> 2);
    uint32_t green_8 = (green_6 << 2) | (green_6 >> 4);
    uint32_t blue_8  = (blue_5 << 3) | (blue_5 >> 2);

    return (d2_color) ((red_8 << 16) |
                       (green_8 << 8) |
                       blue_8);
}

bool dave2d_overlay_init(void)
{
    if (gp_d2_device != NULL)
    {
        return true;
    }

    gp_d2_device = d2_opendevice(0);

    if (gp_d2_device == NULL)
    {
        g_d2_last_error = D2_INVALIDDEVICE;
        return false;
    }

    g_d2_last_error = d2_inithw(gp_d2_device, 0);

    if (g_d2_last_error != D2_OK)
    {
        d2_closedevice(gp_d2_device);
        gp_d2_device = NULL;
        return false;
    }

    gp_d2_render_buffer =
        d2_newrenderbuffer(gp_d2_device,
                           DAVE2D_RENDER_BUFFER_INITIAL_SIZE,
                           DAVE2D_RENDER_BUFFER_STEP_SIZE);

    if (gp_d2_render_buffer == NULL)
    {
        g_d2_last_error = D2_NOMEMORY;
        d2_closedevice(gp_d2_device);
        gp_d2_device = NULL;
        return false;
    }

    /*
     * 纯色、不透明、直接覆盖目标像素。
     */
    g_d2_last_error =
        d2_selectrendermode(gp_d2_device, d2_rm_solid);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    g_d2_last_error =
        d2_setblendmode(gp_d2_device, d2_bm_one, d2_bm_zero);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    return true;
}

bool dave2d_overlay_draw_test_rect(void * framebuffer,
                                   int framebuffer_width,
                                   int framebuffer_height,
                                   int framebuffer_pitch_pixels)
{
    if ((gp_d2_device == NULL) ||
        (gp_d2_render_buffer == NULL) ||
        (framebuffer == NULL) ||
        (framebuffer_width <= 0) ||
        (framebuffer_height <= 0) ||
        (framebuffer_pitch_pixels < framebuffer_width))
    {
        g_d2_last_error = D2_INVALIDDEVICE;
        return false;
    }

    /*
     * 重新选择 render buffer。
     *
     * 上一轮 execute 后，必须重新 select，
     * 才能清空并重新录入本轮命令。
     */
    g_d2_last_error =
        d2_selectrenderbuffer(gp_d2_device, gp_d2_render_buffer);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    /*
     * pitch 的单位是“像素”，不是字节。
     *
     * 对于 1024×600 RGB565：
     *     framebuffer_pitch_pixels = 1024
     * 而不是 2048。
     */
    g_d2_last_error =
        d2_framebuffer(gp_d2_device,
                       framebuffer,
                       framebuffer_pitch_pixels,
                       (d2_u32) framebuffer_width,
                       (d2_u32) framebuffer_height,
                       d2_mode_rgb565);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    g_d2_last_error =
        d2_cliprect(gp_d2_device,
                    0,
                    0,
                    framebuffer_width - 1,
                    framebuffer_height - 1);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    /* 测试颜色：RGB565 红色 0xF800。 */
    g_d2_last_error =
        d2_setcolor(gp_d2_device,
                    0,
                    rgb565_to_d2_color(0xF800U));

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    /*
     * DAVE 2D 坐标使用 4 bit 小数定点数。
     *
     * D2_FIX4(100) 实际上传给硬件的是 100 × 16。
     */
    g_d2_last_error =
        d2_renderline(gp_d2_device,
                      D2_FIX4(TEST_RECT_X0),
                      D2_FIX4(TEST_RECT_Y0),
                      D2_FIX4(TEST_RECT_X1),
                      D2_FIX4(TEST_RECT_Y0),
                      D2_FIX4(TEST_RECT_LINE_WIDTH),
                      d2_le_exclude_none);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    g_d2_last_error =
        d2_renderline(gp_d2_device,
                      D2_FIX4(TEST_RECT_X1),
                      D2_FIX4(TEST_RECT_Y0),
                      D2_FIX4(TEST_RECT_X1),
                      D2_FIX4(TEST_RECT_Y1),
                      D2_FIX4(TEST_RECT_LINE_WIDTH),
                      d2_le_exclude_none);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    g_d2_last_error =
        d2_renderline(gp_d2_device,
                      D2_FIX4(TEST_RECT_X1),
                      D2_FIX4(TEST_RECT_Y1),
                      D2_FIX4(TEST_RECT_X0),
                      D2_FIX4(TEST_RECT_Y1),
                      D2_FIX4(TEST_RECT_LINE_WIDTH),
                      d2_le_exclude_none);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    g_d2_last_error =
        d2_renderline(gp_d2_device,
                      D2_FIX4(TEST_RECT_X0),
                      D2_FIX4(TEST_RECT_Y1),
                      D2_FIX4(TEST_RECT_X0),
                      D2_FIX4(TEST_RECT_Y0),
                      D2_FIX4(TEST_RECT_LINE_WIDTH),
                      d2_le_exclude_none);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    /*
     * 把刚才录入的命令提交给 DAVE 2D。
     */
    g_d2_last_error =
        d2_executerenderbuffer(gp_d2_device,
                               gp_d2_render_buffer,
                               0);

    if (g_d2_last_error != D2_OK)
    {
        return false;
    }

    /*
     * 等待硬件确实完成。
     *
     * 第一步故意采用同步等待，确保 GLCDC 不会提前显示
     * 一张尚未绘制完成的 framebuffer。
     */
    g_d2_last_error = d2_flushframe(gp_d2_device);

    return (g_d2_last_error == D2_OK);
}

int32_t dave2d_overlay_get_last_error(void)
{
    return g_d2_last_error;
}