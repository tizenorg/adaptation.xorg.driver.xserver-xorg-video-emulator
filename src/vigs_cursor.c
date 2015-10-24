/*
 * X.Org X server driver for VIGS
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact :
 * Vasiliy Ulyanov<v.ulyanov@samsung.com>
 * Jinhyung Jo <jinhyung.jo@samsung.com>
 * Sangho Park <sangho1206.park@samsung.com>
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

#include "vigs_cursor.h"
#include "vigs_screen.h"
#include "vigs_drm.h"
#include "vigs_drm_plane.h"
#include "vigs_xv_overlay.h"
#include "vigs_log.h"
#include "vigs.h"
#include <drm_fourcc.h>

#define VIGS_CURSOR_WIDTH 128
#define VIGS_CURSOR_HEIGHT 128
#define VIGS_CURSOR_ZPOS 2

Bool vigs_cursor_init(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    struct vigs_drm_plane *plane;
    struct vigs_cursor *cursor;
    int ret;

    VIGS_LOG_TRACE("enter");

    cursor = vigs_screen->cursor = xnfcalloc(sizeof(*cursor), 1);

    cursor->screen = vigs_screen;

    ret = vigs_drm_surface_create(vigs_screen->drm->dev,
                                  VIGS_CURSOR_WIDTH,
                                  VIGS_CURSOR_HEIGHT,
                                  VIGS_CURSOR_WIDTH * 4,
                                  vigs_drm_surface_bgra8888,
                                  0,
                                  &cursor->sfc);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex,
                   X_ERROR,
                   "Unable to create cursor surface: %s\n", strerror(-ret));
        goto fail_surface_create;
    }

    VIGS_LOG_DEBUG("cursor surface created");

    ret = vigs_drm_gem_map(&cursor->sfc->gem, 1);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex,
                   X_ERROR,
                   "Unable to map cursor surface: %s\n", strerror(-ret));
        goto fail_surface_map;
    }

    VIGS_LOG_DEBUG("cursor surface mapped");

    vigs_list_last(struct vigs_drm_plane,
                   plane,
                   &vigs_screen->drm->planes,
                   list);

    cursor->overlay = vigs_xv_overlay_create(plane);

    if (!cursor->overlay) {
        xf86DrvMsg(scrn->scrnIndex,
                   X_ERROR,
                   "Unable to create cursor overlay\n");
        goto fail_overlay_create;
    }

    if (!xf86_cursors_init(scrn->pScreen, VIGS_CURSOR_WIDTH, VIGS_CURSOR_HEIGHT,
                           (HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                            HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
                            HARDWARE_CURSOR_INVERT_MASK |
                            HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
                            HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
                            HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64 |
                            HARDWARE_CURSOR_UPDATE_UNHIDDEN |
                            HARDWARE_CURSOR_ARGB))) {
        xf86DrvMsg(scrn->scrnIndex,
                   X_ERROR,
                   "Unable to initialize hardware cursor\n");
        goto fail_cursors_init;
    }

    VIGS_LOG_TRACE("HWCursor initialized");

    return TRUE;

fail_cursors_init:
    vigs_xv_overlay_destroy(cursor->overlay);
    cursor->overlay = NULL;

fail_overlay_create:
    vigs_drm_gem_unmap(&cursor->sfc->gem);

fail_surface_map:
    vigs_drm_gem_unref(&cursor->sfc->gem);
    cursor->sfc = NULL;

fail_surface_create:
    vigs_screen->cursor = NULL;
    free(cursor);

    return FALSE;
}

void vigs_cursor_close(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    struct vigs_cursor *cursor = vigs_screen->cursor;

    VIGS_LOG_TRACE("enter");

    if (!cursor)
        return;

    xf86_cursors_fini(scrn->pScreen);

    vigs_xv_overlay_destroy(cursor->overlay);
    cursor->overlay = NULL;

    vigs_drm_gem_unmap(&cursor->sfc->gem);
    vigs_drm_gem_unref(&cursor->sfc->gem);
    cursor->sfc = NULL;

    free(cursor);
    vigs_screen->cursor = NULL;
}

void vigs_cursor_load_argb(struct vigs_cursor *cursor, void *image)
{
    struct vigs_drm_surface *sfc = cursor->sfc;
    int i, ret;

    ret = vigs_drm_surface_start_access(sfc, VIGS_DRM_SAF_WRITE);

    if (ret == 0) {
        VIGS_LOG_DEBUG("size = %d, dst = %p, src = %p",
                       sfc->gem.size,
                       sfc->gem.vaddr,
                       image);

        memcpy(sfc->gem.vaddr, image, sfc->gem.size);

        ret = vigs_drm_surface_end_access(sfc, 1);

        if (ret) {
            VIGS_LOG_ERROR("Unable to end GEM access: %s\n", strerror(-ret));
        }
    } else {
        VIGS_LOG_ERROR("Unable to start GEM access: %s\n", strerror(-ret));
    }
}

void vigs_cursor_set_position(struct vigs_cursor *cursor, int x, int y,
                              int update)
{
    xRectangle src = { 0, 0, VIGS_CURSOR_WIDTH, VIGS_CURSOR_HEIGHT };
    xRectangle dst = { x, y, VIGS_CURSOR_WIDTH, VIGS_CURSOR_HEIGHT };

    VIGS_LOG_DEBUG("x = %d, y = %d, update = %d", x, y, update);

    cursor->x = x;
    cursor->y = y;

    if (update) {
        vigs_xv_overlay_update(cursor->overlay,
                               &src,
                               &dst,
                               VIGS_CURSOR_ZPOS,
                               0 /* hflip */,
                               0 /* vflip */,
                               0 /*rotation */);
    }
}

void vigs_cursor_show(struct vigs_cursor *cursor)
{
    VIGS_LOG_DEBUG("sfc = %p, x = %d, y = %d",
                   cursor->sfc,
                   cursor->x,
                   cursor->y);

    vigs_xv_overlay_enable_surface(cursor->overlay, cursor->sfc);
}

void vigs_cursor_hide(struct vigs_cursor *cursor)
{
    VIGS_LOG_DEBUG("x = %d, y = %d", cursor->x, cursor->y);

    vigs_xv_overlay_disable(cursor->overlay);
}
