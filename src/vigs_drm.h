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

#ifndef _VIGS_DRM_H_
#define _VIGS_DRM_H_

#include "vigs_config.h"
#include "vigs_list.h"
#include "xf86.h"
#include "xf86drmMode.h"
#include "xf86drm.h"

struct vigs_drm_device;
struct vigs_drm_surface;
struct vigs_screen;
struct vigs_dri2_frame_event;

struct vigs_drm
{
    struct vigs_screen *screen;

    int fd;

    char* drm_device_name;

    struct vigs_drm_device *dev;

    drmModeResPtr mode_res;

    drmModePlaneResPtr plane_res;

    struct vigs_list planes;

    drmEventContext event_context;

    uint32_t fb_id;
};

Bool vigs_drm_pre_init(struct vigs_screen *vigs_screen,
                       const char *bus_id);

Bool vigs_drm_init(struct vigs_screen *vigs_screen);

void vigs_drm_close(struct vigs_screen *vigs_screen);

void vigs_drm_free(struct vigs_screen *vigs_screen);

Bool vigs_drm_set_master(struct vigs_drm *drm);

void vigs_drm_drop_master(struct vigs_drm *drm);

Bool vigs_drm_pageflip(struct vigs_drm *drm,
                       struct vigs_dri2_frame_event *frame_event,
                       struct vigs_drm_surface *sfc);

#endif
