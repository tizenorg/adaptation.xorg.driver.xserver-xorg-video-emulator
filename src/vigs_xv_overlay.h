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

#ifndef _VIGS_XV_OVERLAY_H_
#define _VIGS_XV_OVERLAY_H_

#include "vigs_config.h"
#include "xf86.h"

struct vigs_screen;
struct vigs_drm_plane;
struct vigs_drm_surface;

struct vigs_xv_overlay
{
    struct vigs_drm_plane *plane;

    struct vigs_drm_surface *sfc;

    uint32_t fb_id;
};

struct vigs_xv_overlay *vigs_xv_overlay_create(struct vigs_drm_plane *plane);

void vigs_xv_overlay_destroy(struct vigs_xv_overlay *overlay);

Bool vigs_xv_overlay_enable(struct vigs_xv_overlay *overlay,
                            uint32_t width, uint32_t height);

Bool vigs_xv_overlay_enable_surface(struct vigs_xv_overlay *overlay,
                                    struct vigs_drm_surface *sfc);

Bool vigs_xv_overlay_enabled(struct vigs_xv_overlay *overlay);

Bool vigs_xv_overlay_update(struct vigs_xv_overlay *overlay,
                            const xRectangle *src_rect,
                            const xRectangle *dst_rect,
                            int zpos,
                            int hflip,
                            int vflip,
                            int rotation);

void vigs_xv_overlay_disable(struct vigs_xv_overlay *overlay);

#endif
