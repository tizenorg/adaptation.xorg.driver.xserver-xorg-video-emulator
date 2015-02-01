/*
 * X.Org X server driver for VIGS
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact :
 * Stanislav Vorobiov <s.vorobiov@samsung.com>
 * Jinhyung Jo <jinhyung.jo@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#ifndef _VIGS_UTILS_H_
#define _VIGS_UTILS_H_

#include "vigs_config.h"
#include "xf86drm.h"
#include "xf86drmMode.h"
#include "xf86Crtc.h"
#include <pixman.h>

#define vigs_offsetof(type, member) ((size_t)&((type*)0)->member)

#define vigs_containerof(ptr, type, member) ((type*)((char*)(ptr) - vigs_offsetof(type, member)))

#define vigs_max(a,b) (((a) > (b)) ? (a) : (b))
#define vigs_min(a,b) (((a) < (b)) ? (a) : (b))

void vigs_drm_mode_to_mode(ScrnInfoPtr scrn,
                           drmModeModeInfo *drm_mode,
                           DisplayModePtr mode);

void vigs_mode_to_drm_mode(ScrnInfoPtr scrn,
                           DisplayModePtr mode,
                           drmModeModeInfo *drm_mode);

PropertyPtr vigs_get_window_property(WindowPtr window,
                                     const char *property_name);

PixmapPtr vigs_get_drawable_pixmap(DrawablePtr drawable);

void vigs_pixman_convert_image(pixman_op_t op,
                               unsigned char *srcbuf,
                               unsigned char *dstbuf,
                               pixman_format_code_t src_format,
                               pixman_format_code_t dst_format,
                               xRectangle *img_rect,
                               xRectangle *pxm_rect,
                               xRectangle *src_rect,
                               xRectangle *dst_rect,
                               RegionPtr clip_region,
                               int rotation,
                               int is_hflip,
                               int is_vflip);

uint64_t vigs_gettime_us();

void *vigs_copy_image(int width, int height,
                      char *s, int s_size_w, int s_size_h,
                      int *s_pitches, int *s_offsets, int *s_lengths,
                      char *d, int d_size_w, int d_size_h,
                      int *d_pitches, int *d_offsets, int *d_lengths,
                      int channel, int h_sampling, int v_sampling);

#endif
