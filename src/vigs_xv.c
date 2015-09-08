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

#include "vigs_xv.h"
#include "vigs_xv_image_out.h"
#include "vigs_xv_in.h"
#include "vigs_screen.h"
#include "vigs_xv_adaptor.h"
#include "vigs_xv_overlay.h"
#include "vigs_drm.h"
#include "vigs_drm_plane.h"
#include "vigs_log.h"
#include <X11/Xatom.h>

static Bool g_block_handler_registered = FALSE;

static Bool vigs_xv_set_hw_ports_property(ScreenPtr screen, int num)
{
    WindowPtr window = screen->root;
    Atom atom;

    if (!window || !serverClient) {
        return FALSE;
    }

    atom = MakeAtom("X_HW_PORTS", strlen("X_HW_PORTS"), TRUE);

    dixChangeWindowProperty(serverClient,
                            window, atom, XA_CARDINAL, 32,
                            PropModeReplace, 1, (unsigned int*)&num, FALSE);

    return TRUE;
}

static void vigs_xv_block_handler(pointer data,
                                  OSTimePtr timeout,
                                  pointer read)
{
    ScrnInfoPtr scrn = (ScrnInfoPtr)data;
    struct vigs_screen *vigs_screen = scrn->driverPrivate;

    if (g_block_handler_registered &&
        vigs_xv_set_hw_ports_property(scrn->pScreen, vigs_screen->xv->num_overlays))
    {
        RemoveBlockAndWakeupHandlers(&vigs_xv_block_handler,
                                     (WakeupHandlerProcPtr)NoopDDA,
                                     data);
        g_block_handler_registered = FALSE;
    }
}

Bool vigs_xv_init(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    struct vigs_xv *xv;
    struct vigs_xv_adaptor *adaptor;
    struct vigs_drm_plane *plane;
    int i = 0;

    VIGS_LOG_TRACE("enter");

    xv = vigs_screen->xv = xnfcalloc(sizeof(*xv), 1);

    xv->screen = vigs_screen;

    xv->num_overlays = vigs_screen->drm->plane_res->count_planes;
    xv->overlays = calloc(xv->num_overlays, sizeof(*xv->overlays));

    if (!xv->overlays) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate overlays\n");
        return FALSE;
    }

    vigs_list_for_each(struct vigs_drm_plane,
                       plane,
                       &vigs_screen->drm->planes,
                       list) {
        xv->overlays[i] = vigs_xv_overlay_create(plane);

        if (!xv->overlays[i]) {
            return FALSE;
        }

        ++i;
    }

    adaptor = vigs_xv_image_out_create(xv);

    if (!adaptor) {
        return FALSE;
    }

    xv->adaptors[0] = &adaptor->base;

    adaptor = vigs_xv_in_create(xv);

    if (!adaptor) {
        return FALSE;
    }

    xv->adaptors[1] = &adaptor->base;

    if (!xf86XVScreenInit(scrn->pScreen, &xv->adaptors[0], VIGS_NUM_XV_ADAPTORS)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "xf86XVScreenInit failed\n");
        return FALSE;
    }

    if (!g_block_handler_registered) {
        RegisterBlockAndWakeupHandlers(&vigs_xv_block_handler,
                                       (WakeupHandlerProcPtr)NoopDDA,
                                       scrn);
        g_block_handler_registered = TRUE;
    }

    VIGS_LOG_TRACE("Xv initialized");

    return TRUE;
}

void vigs_xv_close(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    struct vigs_xv *xv = vigs_screen->xv;
    int i;

    VIGS_LOG_TRACE("enter");

    for (i = 0; i < VIGS_NUM_XV_ADAPTORS; ++i) {
        struct vigs_xv_adaptor *adaptor = (struct vigs_xv_adaptor*)xv->adaptors[i];

        adaptor->destroy(adaptor);
        xv->adaptors[i] = NULL;
    }

    for (i = 0; i < xv->num_overlays; ++i) {
        if (vigs_xv_overlay_enabled(xv->overlays[i])) {
            xf86DrvMsg(scrn->scrnIndex, X_WARNING, "Overlay %u is still enabled after all adapter have been closed\n",
                       xv->overlays[i]->plane->mode_plane->plane_id);
        }

        vigs_xv_overlay_destroy(xv->overlays[i]);
        xv->overlays[i] = NULL;
    }

    free(xv->overlays);
    xv->overlays = NULL;

    free(xv);
    vigs_screen->xv = NULL;
}
