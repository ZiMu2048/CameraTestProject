#ifndef DAVE2D_OVERLAY_H
#define DAVE2D_OVERLAY_H

#include "hal_data.h"
#include <stdbool.h>
#include <stdint.h>

bool dave2d_overlay_init(void);
bool dave2d_overlay_draw_test_rect(void * framebuffer,
                                   int framebuffer_width,
                                   int framebuffer_height,
                                   int framebuffer_pitch_pixels);
int32_t dave2d_overlay_get_last_error(void);


#endif // DAVE2D_OVERLAY_H