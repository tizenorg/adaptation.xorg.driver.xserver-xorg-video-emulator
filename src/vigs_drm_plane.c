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

#include "vigs_drm_plane.h"
#include "vigs_drm.h"
#include "vigs_log.h"

Bool vigs_drm_plane_init(struct vigs_drm *drm, int num)
{
    struct vigs_drm_plane *plane;

    VIGS_LOG_TRACE("%d", num);

    plane = xnfcalloc(sizeof(*plane), 1);

    vigs_list_init(&plane->list);
    plane->drm = drm;
    plane->num = num;
    plane->mode_plane = drmModeGetPlane(drm->fd,
                                        drm->plane_res->planes[num]);

    if (!plane->mode_plane) {
        return FALSE;
    }

    vigs_list_add_tail(&drm->planes, &plane->list);

    return TRUE;
}

void vigs_drm_plane_destroy(struct vigs_drm_plane *plane)
{
    VIGS_LOG_TRACE("%d", plane->num);

    vigs_list_remove(&plane->list);

    drmModeFreePlane(plane->mode_plane);

    free(plane);
}
