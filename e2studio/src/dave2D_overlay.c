#include "dave2D_overlay.h"
#include "dave_driver.h"
#include "dave_math.h"

#define DAVE2D_RENDER_BUFFER_INITIAL_SIZE    (256U)
#define DAVE2D_RENDER_BUFFER_STEP_SIZE       (128U)

/* 当前绘制帧的有效尺寸，由 begin() 保存，供后续坐标裁剪使用。 */
static int  g_framebuffer_width;
static int  g_framebuffer_height;
/* 软件绘制状态：true 表示当前处于 begin() 与 end() 之间的命令录制阶段。 */
static bool g_frame_active;

/* D/AVE 2D 设备句柄。 */
static d2_device       * gp_d2_device = NULL;
/* 保存一帧内批量绘图命令的 render buffer。 */
static d2_renderbuffer * gp_d2_render_buffer = NULL;
/* 保存最近一次适配层检查或 D/AVE 2D API 返回的错误码。 */
static int32_t g_d2_last_error = D2_OK;

/***********************************************************************************************************************
 * 函数名称：rgb565_to_d2_color
 * 功能说明：把 framebuffer 使用的 RGB565 颜色转换为 D/AVE 2D 接口使用的 0x00RRGGBB 格式。
 *           通过复制高位的方式将 5/6 bit 色彩分量扩展到 8 bit，尽量覆盖完整的 0～255 范围。
 * 输入参数：
 *   rgb565 - 16 bit RGB565 颜色值
 * 返回值：
 *   转换后的 D/AVE 2D 颜色值
 **********************************************************************************************************************/
static d2_color rgb565_to_d2_color(uint16_t rgb565)
{
    uint32_t red_5   = (rgb565 >> 11) & 0x1FU;
    uint32_t green_6 = (rgb565 >> 5)  & 0x3FU;
    uint32_t blue_5  = rgb565 & 0x1FU;

    /* 将 5/6 bit 色彩分量扩展到 8 bit。 */
    uint32_t red_8   = (red_5 << 3) | (red_5 >> 2);
    uint32_t green_8 = (green_6 << 2) | (green_6 >> 4);
    uint32_t blue_8  = (blue_5 << 3) | (blue_5 >> 2);

    return (d2_color) ((red_8 << 16) |
                       (green_8 << 8) |
                       blue_8);
}

/***********************************************************************************************************************
 * 函数名称：dave2d_overlay_init
 * 功能说明：创建并初始化 D/AVE 2D 设备及 render buffer，同时配置纯色、不透明覆盖绘制模式。
 *           设备实例属于整个应用，而不是某一帧，因此本函数具有重复调用保护，只初始化一次。(宏定义效果)
 * 输入参数：无
 * 返回值：
 *   true  - 初始化成功，或设备此前已经初始化
 *   false - 设备打开、硬件初始化、命令缓冲区分配或绘制模式设置失败；
 *           可通过 dave2d_overlay_get_last_error() 查询具体错误码
 **********************************************************************************************************************/
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

/***********************************************************************************************************************
 * 函数名称：dave2d_overlay_get_last_error
 * 功能说明：返回适配层最近一次参数/状态检查或 D/AVE 2D API 调用产生的错误码。
 * 输入参数：无
 * 返回值：
 *   D2_OK 表示最近一次操作成功，其他值对应 dave_errorcodes.h 中定义的错误原因
 **********************************************************************************************************************/
int32_t dave2d_overlay_get_last_error(void)
{
    return g_d2_last_error;
}

/***********************************************************************************************************************
 * 函数名称：dave2d_overlay_begin
 * 功能说明：开始一帧 D/AVE 2D 叠加绘制，选择本帧命令缓冲区、绑定目标 framebuffer，
 *           并设置有效绘图裁剪区域。本函数只负责准备和录制环境，不会立即执行硬件绘图命令。(申请命令表)
 * 输入参数：
 *   framebuffer  - 本帧需要绘制的目标 framebuffer 首地址
 *   width        - framebuffer 的有效宽度，单位为像素
 *   height       - framebuffer 的有效高度，单位为像素
 *   pitch_pixels - framebuffer 相邻两行起点之间的距离，单位为像素，不是字节
 * 返回值：
 *   true  - 本帧绘制环境准备成功，后续可以调用 dave2d_overlay_draw_rect()
 *   false - 参数无效、设备未初始化或 D/AVE 2D API 调用失败；
 *           可通过 dave2d_overlay_get_last_error() 查询具体错误码
 * 调用约束：
 *   必须与 dave2d_overlay_end() 成对调用；上一帧尚未结束时不能再次调用本函数。
 **********************************************************************************************************************/
bool dave2d_overlay_begin(void * framebuffer,
                          int width,
                          int height,
                          int pitch_pixels)
{
    if(true == g_frame_active)
    {
        g_d2_last_error = D2_DEVICEBUSY;
        return false;
    }
    if(NULL == gp_d2_device)
    {
        g_d2_last_error = D2_INVALIDDEVICE;
        return false;
    }
    if(NULL == gp_d2_render_buffer)
    {
        g_d2_last_error = D2_INVALIDBUFFER;
        return false;
    }
    if(NULL == framebuffer)
    {
        g_d2_last_error = D2_NULLPOINTER;
        return false;
    }
    if(width <= 0)
    {
        g_d2_last_error = D2_INVALIDWIDTH;
        return false;
    }
    if(height <= 0)
    {
        g_d2_last_error = D2_INVALIDHEIGHT;
        return false;
    }
    if(pitch_pixels < width)
    {
        g_d2_last_error = D2_INVALIDWIDTH;
        return false;
    }

    g_d2_last_error = d2_selectrenderbuffer(gp_d2_device, gp_d2_render_buffer);//gp_d2_render_buffer是指令表
    if(D2_OK != g_d2_last_error)
    {
        return false;
    }

    g_d2_last_error =
        d2_framebuffer(gp_d2_device,
                       framebuffer,
                       pitch_pixels,
                       (d2_u32) width,
                       (d2_u32) height,
                       d2_mode_rgb565);
    if(D2_OK != g_d2_last_error)
    {
        return false;
    }

    g_d2_last_error =
        d2_cliprect(gp_d2_device,
                    0,
                    0,
                    (d2_border) (width - 1),
                    (d2_border) (height - 1));
    if(D2_OK != g_d2_last_error)
    {
        return false;
    }

    g_framebuffer_width = width;
    g_framebuffer_height = height;
    g_frame_active = true;

    return true;
}

/***********************************************************************************************************************
 * 函数名称：dave2d_overlay_draw_rect
 * 功能说明：向当前帧的 D/AVE_2D_render_buffer中追加一个矩形框的四条边。
 *           本函数只录制绘图命令，不提交命令，也不等待硬件执行完成。(填写命令表)
 * 输入参数：
 *   x0、y0     - 矩形第一个对角点的像素坐标
 *   x1、y1     - 矩形第二个对角点的像素坐标
 *   rgb565     - 矩形颜色，格式为 RGB565
 *   line_width - 矩形边框宽度，单位为像素
 * 返回值：
 *   true  - 矩形命令录制成功，或矩形完全位于 framebuffer 外而无需绘制
 *   false - 当前不在有效绘制帧内、参数无效或 D/AVE 2D API 调用失败；
 *           可通过 dave2d_overlay_get_last_error() 查询具体错误码
 * 调用约束：
 *   只能在 dave2d_overlay_begin() 成功之后、dave2d_overlay_end() 之前调用；
 *   一帧内可以连续调用多次，将多个矩形批量录入同一个 render buffer。
 **********************************************************************************************************************/
bool dave2d_overlay_draw_rect(int x0,
                              int y0,
                              int x1,
                              int y1,
                              uint16_t rgb565,
                              int line_width)
{
    /*
     * 检查当前是否处于有效的“命令录制阶段”。
     *
     * dave2d_overlay_begin() 成功后会把 g_frame_active 置为 true，
     * 表示 framebuffer 已绑定、裁剪区域已设置，render buffer 可以继续接收绘图命令。
     * draw_rect() 只能读取这个状态，不能在内部再次调用 begin()，否则会重新选择和重置
     * render buffer，破坏同一帧内多个矩形共用一张命令表的批量绘制流程。
     */
    if (gp_d2_device == NULL)
    {
        g_d2_last_error = D2_INVALIDDEVICE;
        return false;
    }

    if (!g_frame_active)
    {
        g_d2_last_error = D2_INVALIDCONTEXT;
        return false;
    }

    /*检查线宽*/
    if (line_width <= 0)
    {
        g_d2_last_error = D2_INVALIDWIDTH;
        return false;
    }

    /*统一坐标方向*/
    if (x0 > x1)
    {
        x0 ^= x1;
        x1 ^= x0;
        x0 ^= x1;
    }

    if (y0 > y1)
    {
        y0 ^= y1;
        y1 ^= y0;
        y0 ^= y1;
    }//统一成左上右下角

    /*判断矩形是否完全在屏幕外*/
    if ((x1 < 0) ||
    (y1 < 0) ||
    (x0 >= g_framebuffer_width) ||
    (y0 >= g_framebuffer_height))
    {
        g_d2_last_error = D2_OK;
        return true;
    }

    /*裁剪到有效区域(framebuffer范围)*/
    if (x0 < 0)                         { x0 = 0; }
    if (y0 < 0)                         { y0 = 0; }
    if (x1 >= g_framebuffer_width)      { x1 = g_framebuffer_width - 1; }
    if (y1 >= g_framebuffer_height)     { y1 = g_framebuffer_height - 1; }

    /*
     * D/AVE 2D 使用带 4 bit 小数的 16 bit 定点坐标。
     * 坐标已经裁剪到 framebuffer 范围，因此转换后的数值位于 d2_point/d2_width 的有效范围内。
     */
    d2_point d2_x0 = (d2_point) D2_FIX4(x0);
    d2_point d2_y0 = (d2_point) D2_FIX4(y0);
    d2_point d2_x1 = (d2_point) D2_FIX4(x1);
    d2_point d2_y1 = (d2_point) D2_FIX4(y1);
    d2_width d2_line_width = (d2_width) D2_FIX4(line_width);

    /*设置颜色*/
    g_d2_last_error = d2_setcolor(gp_d2_device, 0, rgb565_to_d2_color(rgb565));
    if (D2_OK != g_d2_last_error)
    {
        return false;
    }

    /* 绘制上边：(x0, y0) -> (x1, y0) */
    g_d2_last_error = d2_renderline(gp_d2_device,
                                    d2_x0,
                                    d2_y0,
                                    d2_x1,
                                    d2_y0,
                                    d2_line_width,
                                    d2_le_exclude_none);
    if (D2_OK != g_d2_last_error)
    {
        return false;
    }

    /* 绘制右边：(x1, y0) -> (x1, y1) */
    g_d2_last_error = d2_renderline(gp_d2_device,
                                    d2_x1,
                                    d2_y0,
                                    d2_x1,
                                    d2_y1,
                                    d2_line_width,
                                    d2_le_exclude_none);
    if (D2_OK != g_d2_last_error)
    {
        return false;
    }

    /* 绘制下边：(x1, y1) -> (x0, y1) */
    g_d2_last_error = d2_renderline(gp_d2_device,
                                    d2_x1,
                                    d2_y1,
                                    d2_x0,
                                    d2_y1,
                                    d2_line_width,
                                    d2_le_exclude_none);
    if (D2_OK != g_d2_last_error)
    {
        return false;
    }

    /* 绘制左边：(x0, y1) -> (x0, y0) */
    g_d2_last_error = d2_renderline(gp_d2_device,
                                    d2_x0,
                                    d2_y1,
                                    d2_x0,
                                    d2_y0,
                                    d2_line_width,
                                    d2_le_exclude_none);
    if (D2_OK != g_d2_last_error)
    {
        return false;
    }

    return true;
}

/***********************************************************************************************************************
 * 函数名称：dave2d_overlay_end
 * 功能说明：结束当前帧的 D/AVE 2D 命令录制，将同一 render buffer 中累计的全部绘图命令
 *           一次性提交给硬件执行，并等待硬件完成后释放本帧的软件绘制状态。
 * 输入参数：无
 * 返回值：
 *   true  - 本帧全部绘图命令已执行完成，目标 framebuffer 可以交给 GLCDC 显示
 *   false - 设备或 render buffer 无效、当前没有活动绘制帧，或者命令提交/硬件执行失败；
 *           可通过 dave2d_overlay_get_last_error() 查询具体错误码
 * 调用约束：
 *   必须在 dave2d_overlay_begin() 成功之后调用；本函数返回后，本帧绘制会话已经关闭，
 *   下一帧必须重新调用 dave2d_overlay_begin() 才能继续录入绘图命令。
 **********************************************************************************************************************/
bool dave2d_overlay_end(void)
{
    if(NULL == gp_d2_device)
    {
        g_frame_active      = false;
        g_framebuffer_width = 0;
        g_framebuffer_height = 0;
        g_d2_last_error = D2_INVALIDDEVICE;
        return false;
    }
    if(NULL == gp_d2_render_buffer)
    {
        g_frame_active      = false;
        g_framebuffer_width = 0;
        g_framebuffer_height = 0;
        g_d2_last_error = D2_INVALIDBUFFER;
        return false;
    }
    if(!g_frame_active)//检查是否已经成功调用 begin()
    {
        g_d2_last_error = D2_INVALIDCONTEXT;

        g_frame_active      = false;
        g_framebuffer_width = 0;
        g_framebuffer_height = 0;
        return false;
    }

    g_d2_last_error =
    d2_executerenderbuffer(gp_d2_device,
                           gp_d2_render_buffer,
                           0);
    if(D2_OK != g_d2_last_error)
    {
        g_frame_active      = false;
        g_framebuffer_width = 0;
        g_framebuffer_height = 0;
        return false;
    }

    /* 等待硬件完成。 */
    g_d2_last_error = d2_flushframe(gp_d2_device);
    if(D2_OK != g_d2_last_error)
    {
        g_frame_active      = false;
        g_framebuffer_width = 0;
        g_framebuffer_height = 0;
        return false;
    }

    g_frame_active      = false;
    g_framebuffer_width = 0;
    g_framebuffer_height = 0;

    return true;
}
