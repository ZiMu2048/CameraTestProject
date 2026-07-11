/***********************************************************************************************************************
 * File Name    : mipi_csi.h
 * Description  : Contains data structures and functions used in mipi_csi.c.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/

#ifndef MIPI_CSI_H_
#define MIPI_CSI_H_

#include "common_utils.h"
#include "bsp_pin_cfg.h"
#include "camera_sensor.h"
#include "glcdc_display.h"

#define EP_VERSION      ("1.0")
#define MODULE_NAME     "r_mipi_csi"
#define BANNER_INFO     "\r\n********************************************************************************"\
                        "\r\n*   Renesas FSP Example Project for "MODULE_NAME" Module                          *"\
                        "\r\n*   Example Project Version %s                                                *"\
                        "\r\n*   Flex Software Pack Version  %d.%d.%d                                          *"\
                        "\r\n********************************************************************************"\
                        "\r\nRefer to the accompanying .md file for Example Project details and" \
                        "\r\nFSP User's Manual for more information about "MODULE_NAME" driver\r\n"

#define EP_INFO         "\r\nThis example project demonstrates the basic functionality of MIPI CSI on Renesas\r\n"\
                        "RA MCUs using Renesas FSP. The I2C module configures an external camera sensor to\r\n"\
                        "operate with the MIPI interface. Captured image data is received by the MIPI CSI\r\n"\
                        "module, then processed by the VIN module: clipped per ITU-R BT.601, converted from\r\n"\
                        "YCbCr-422 to RGB, scaled to the user-selected resolution, and stored in SDRAM.\r\n"\
                        "The resolution change is handled by the VIN module, allowing dynamic output\r\n"\
                        "resizing without reconfiguring the camera sensor.\r\n\r\n"\
                        "Before choosing the camera mode, the user is prompted via terminal to select the\r\n"\
                        "desired image resolution. After that, the user can choose to capture a live image\r\n"\
                        "or a test pattern by typing '1', '2', or '3' to return to the main menu.\r\n"\
                        "Captured images are viewable as raw data in e2studio using the Memory view.\r\n"\
                        "If a Parallel Graphics LCD is connected, the image is also displayed on-screen.\r\n\r\n"

/* Public functions declarations */
void handle_error (fsp_err_t err,  char *err_str);
void mipi_csi_ep_entry(void);
static void preprocess_frame_to_fomo(const uint8_t *src, int8_t *dst);
static void draw_rect_rgb565(uint8_t *fb, int x0, int y0, int x1, int y1,uint16_t color, int fb_stride_pixels);
#endif /* MIPI_CSI_H_ */
