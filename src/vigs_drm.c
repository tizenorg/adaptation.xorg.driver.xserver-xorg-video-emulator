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

#include "vigs_drm.h"
#include "vigs_drm_crtc.h"
#include "vigs_drm_output.h"
#include "vigs_drm_plane.h"
#include "vigs_screen.h"
#include "vigs_log.h"
#include "vigs_dri2.h"
#include "xf86Crtc.h"
#include "vigs.h"
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

struct vigs_drm_queue
{
    struct vigs_list list;
    uint32_t seq;
    void *data;
    int aborted;
    vigs_drm_handler_proc handler;
    vigs_drm_abort_proc abort;
};

struct vigs_drm_pageflip_data
{
    struct vigs_drm *drm;
    void *data;
    uint32_t old_fb_id;

    vigs_drm_handler_proc handler;
    vigs_drm_abort_proc abort; /* curently unused */
};

struct vigs_drm_queue *vigs_drm_queue_alloc(struct vigs_drm *drm,
                                            void *data,
                                            vigs_drm_handler_proc handler,
                                            vigs_drm_abort_proc abort)
{
    struct vigs_drm_queue *q = calloc(1, sizeof(*q));

    if (q) {
        if (!drm->seq) {
            drm->seq++;
        }

        q->seq = drm->seq++;
        q->data = data;
        q->handler = handler;
        q->abort = abort;
        vigs_list_add(&drm->queue, &q->list);
    }

    return q;
}

uint32_t vigs_drm_queue_seq(struct vigs_drm_queue *q)
{
    return q->seq;
}

void vigs_drm_queue_abort_one(struct vigs_drm_queue *q)
{
    q->aborted = true;

    if (q->abort) {
        q->abort(q->data);
    }
}

void vigs_drm_queue_abort(struct vigs_drm *drm,
                          Bool (*match)(void *data, void *match_data),
                          void *match_data)
{
    struct vigs_drm_queue *q, *tmp;

    vigs_list_for_each_safe(struct vigs_drm_queue, q, tmp, &drm->queue, list) {
        if (match(q->data, match_data)) {
            vigs_drm_queue_abort_one(q);
            break;
        }
    }
}

void vigs_drm_queue_abort_seq(struct vigs_drm *drm, uint32_t seq)
{
    struct vigs_drm_queue *q, *tmp;

    vigs_list_for_each_safe(struct vigs_drm_queue, q, tmp, &drm->queue, list) {
        if (q->seq == seq) {
            vigs_drm_queue_abort_one(q);
            break;
        }
    }
}

static void vigs_drm_queue_drop(struct vigs_drm *drm)
{
    struct vigs_drm_queue *q, *tmp;
    int cnt = 0;

    vigs_list_for_each_safe(struct vigs_drm_queue, q, tmp, &drm->queue, list) {
        vigs_list_remove(&q->list);
        free(q);
        cnt++;
    }

    VIGS_LOG_INFO("cnt = %d", cnt);
}

int vigs_drm_read_events(struct vigs_drm *drm)
{
    struct pollfd p = {
        .fd = drm->fd,
        .events = POLLIN
    };
    int ret;

    do {
        ret = poll(&p, 1, 0);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

    if (ret <= 0) {
        return 0;
    }

    return drmHandleEvent(drm->fd, &drm->event_context);
}

static Bool vigs_drm_has_pending_events(int fd)
{
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = POLLIN;

    return (poll(&pfd, 1, 0) == 1);
}

static void vigs_drm_vblank_handler(int fd,
                                    unsigned int sequence,
                                    unsigned int tv_sec,
                                    unsigned int tv_usec,
                                    void *user_data)
{
    struct vigs_drm_queue *q = user_data;

    VIGS_LOG_TRACE("sequence = %d, tv_sec = %d, tv_usec = %d",
                   sequence,
                   tv_sec,
                   tv_usec);

    if (q) {
        vigs_list_remove(&q->list);

        if (q->handler && !q->aborted) {
            q->handler(q->data, sequence, tv_sec, tv_usec);
        }
    }

    free(q);
}

static void vigs_drm_page_flip_handler(int fd,
                                       unsigned int sequence,
                                       unsigned int tv_sec,
                                       unsigned int tv_usec,
                                       void *user_data)
{
    struct vigs_drm_pageflip_data *pageflip_data = user_data;
    struct vigs_drm *drm = pageflip_data->drm;
    void *data = pageflip_data->data;
    uint32_t old_fb_id = pageflip_data->old_fb_id;
    vigs_drm_handler_proc handler = pageflip_data->handler;

    VIGS_LOG_TRACE("sequence = %d, tv_sec = %d, tv_usec = %d",
                   sequence,
                   tv_sec,
                   tv_usec);

    free(pageflip_data);

    drmModeRmFB(drm->fd, old_fb_id);

    if (handler) {
        handler(data, sequence, tv_sec, tv_usec);
    }
}

static void vigs_drm_wakeup_handler(pointer block_data,
                                    int result,
                                    pointer read_mask_arg)
{
    struct vigs_drm *drm = block_data;
    fd_set *read_mask = read_mask_arg;

    if ((block_data == NULL) || (result < 0)) {
        return;
    }

    if (FD_ISSET(drm->fd, read_mask)) {
        drmHandleEvent(drm->fd, &drm->event_context);
    }
}

static Bool vigs_drm_crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);

    VIGS_LOG_TRACE("enter: %dx%d", width, height);

    if ((scrn->virtualX == width) &&
        (scrn->virtualY == height)) {
        VIGS_LOG_TRACE("same dimensions, not resizing");
        return TRUE;
    }

    return FALSE;
}

static int vigs_drm_is_render_node(int fd, struct stat *st)
{
    if (fstat(fd, st)) {
        return 0;
    }

    if (!S_ISCHR(st->st_mode)) {
        return 0;
    }

    return (st->st_rdev & 0x80);
}

static char *vigs_drm_find_render_node(int fd)
{
    struct stat master, render;
    char buf[128];

    /* Are we a render-node ourselves? */
    if (vigs_drm_is_render_node(fd, &master)) {
        return NULL;
    }

    sprintf(buf, "/dev/dri/renderD%d", (int)((master.st_rdev | 0x80) & 0xff));

    if (stat(buf, &render) == 0 &&
        master.st_mode == render.st_mode &&
        render.st_rdev == (master.st_rdev | 0x80)) {
        return strdup(buf);
    }

    return NULL;
}

static int vigs_drm_authorise(struct vigs_drm *drm, int fd)
{
    struct stat st;
    drm_magic_t magic;

    if (vigs_drm_is_render_node(fd, &st)) {
        return 1;
    }

    return (drmGetMagic(fd, &magic) == 0 && drmAuthMagic(drm->fd, magic) == 0);
}

int vigs_drm_get_client_fd(struct vigs_drm *drm)
{
    int flags, fd = -1;

    assert(dev && dev->fd != -1);
    assert(dev->drm_render_node);

#ifdef O_CLOEXEC
    fd = open(drm->drm_render_node, O_RDWR | O_CLOEXEC);
#endif

    if (fd < 0) {
        fd = open(drm->drm_render_node, O_RDWR);

        if (fd != -1) {
            flags = fcntl(fd, F_GETFD);

            if (flags != -1) {
                flags |= FD_CLOEXEC;
                fcntl(fd, F_SETFD, flags);
            }
        }
    }

    if (fd < 0) {
        return -BadAlloc;
    }

    if (!vigs_drm_authorise(drm, fd)) {
        close(fd);
        return -BadMatch;
    }

    return fd;
}

static const xf86CrtcConfigFuncsRec vigs_drm_crtc_config_funcs =
{
    .resize = vigs_drm_crtc_resize
};

Bool vigs_drm_pre_init(struct vigs_screen *vigs_screen,
                       const char *bus_id)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    struct vigs_drm *drm;
    int i;
    int ret;

    VIGS_LOG_TRACE("enter");

    drm = vigs_screen->drm = xnfcalloc(sizeof(*drm), 1);

    drm->screen = vigs_screen;

    drm->fd = drmOpen(NULL, bus_id);

    if (drm->fd < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to open DRM\n");
        return FALSE;
    }

    drm->drm_device_name = drmGetDeviceNameFromFd(drm->fd);

    if (!drm->drm_device_name) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to get DRM device name\n");
        return FALSE;
    }

    drm->drm_render_node = vigs_drm_find_render_node(drm->fd);

    if (!drm->drm_render_node) {
        xf86DrvMsg(scrn->scrnIndex, X_WARNING,
                   "Unable to find render node. Using device node instead\n");
        drm->drm_render_node = drm->drm_device_name;
    }

    xf86DrvMsg(scrn->scrnIndex, X_INFO, "Device node: %s\n",
               drm->drm_device_name);
    xf86DrvMsg(scrn->scrnIndex, X_INFO, "Render node: %s\n",
               drm->drm_render_node);

    ret = vigs_drm_device_create(drm->fd, &drm->dev);

    if (ret != 0) {
        if (ret == -EINVAL) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "VIGS DRM kernel interface version mismatch\n");
        } else {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to open VIGS DRM device: %s\n", strerror(-ret));
        }
        return FALSE;
    }

    xf86CrtcConfigInit(scrn, &vigs_drm_crtc_config_funcs);

    drm->mode_res = drmModeGetResources(drm->fd);

    if (!drm->mode_res) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to get DRM resources\n");
        return FALSE;
    }

    drm->plane_res = drmModeGetPlaneResources(drm->fd);

    if (!drm->plane_res) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to get DRM plane resources\n");
        return FALSE;
    }

    vigs_list_init(&drm->planes);

    xf86CrtcSetSizeRange(scrn, 80, 50,
                         drm->mode_res->max_width,
                         drm->mode_res->max_height);

    for (i = 0; i < drm->mode_res->count_crtcs; i++) {
        if (!vigs_drm_crtc_init(drm, i)) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to init CRTC %d\n", i);
            return FALSE;
        }
    }

    for (i = 0; i < drm->mode_res->count_connectors; i++) {
        if (!vigs_drm_output_init(drm, i)) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to init output %d\n", i);
            return FALSE;
        }
    }

    for (i = 0; i < (int)drm->plane_res->count_planes; i++) {
        if (!vigs_drm_plane_init(drm, i)) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to init plane %d\n", i);
            return FALSE;
        }
    }

    if (!xf86InitialConfiguration(scrn, TRUE)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to perform initial configuration\n");
        return FALSE;
    }

    vigs_list_init(&drm->queue);
    drm->seq = 0;

    drm->event_context.version = DRM_EVENT_CONTEXT_VERSION;
    drm->event_context.vblank_handler = vigs_drm_vblank_handler;
    drm->event_context.page_flip_handler = vigs_drm_page_flip_handler;

    VIGS_LOG_TRACE("DRM initialized");

    return TRUE;
}

Bool vigs_drm_init(struct vigs_screen *vigs_screen)
{
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(vigs_screen->scrn);
    struct vigs_drm *drm = vigs_screen->drm;
    int i;

    VIGS_LOG_TRACE("enter");

    if (!vigs_drm_set_master(drm)) {
        return FALSE;
    }

    /*
     * Need to point to new screen on server regeneration.
     */
    for (i = 0; i < crtc_config->num_crtc; i++) {
        crtc_config->crtc[i]->scrn = vigs_screen->scrn;
    }

    for (i = 0; i < crtc_config->num_output; i++) {
        crtc_config->output[i]->scrn = vigs_screen->scrn;
    }

    AddGeneralSocket(drm->fd);
    RegisterBlockAndWakeupHandlers((BlockHandlerProcPtr)NoopDDA,
                                   vigs_drm_wakeup_handler,
                                   drm);

    return TRUE;
}

void vigs_drm_close(struct vigs_screen *vigs_screen)
{
    struct vigs_drm *drm = vigs_screen->drm;

    VIGS_LOG_TRACE("enter");

    while (vigs_drm_has_pending_events(drm->fd)) {
        drmHandleEvent(drm->fd, &drm->event_context);
    }

    RemoveBlockAndWakeupHandlers((BlockHandlerProcPtr)NoopDDA,
                                 vigs_drm_wakeup_handler,
                                 drm);
    RemoveGeneralSocket(drm->fd);

    if (vigs_screen->drm->fb_id != 0) {
        drmModeRmFB(vigs_screen->drm->fd, vigs_screen->drm->fb_id);
        vigs_screen->drm->fb_id = 0;
    }
}

void vigs_drm_free(struct vigs_screen *vigs_screen)
{
    struct vigs_drm_plane *plane, *next;

    vigs_list_for_each_safe(struct vigs_drm_plane,
                            plane,
                            next,
                            &vigs_screen->drm->planes,
                            list) {
        vigs_drm_plane_destroy(plane);
    }

    vigs_drm_device_destroy(vigs_screen->drm->dev);
    vigs_screen->drm->dev = NULL;

    if (vigs_screen->drm->drm_render_node != vigs_screen->drm->drm_device_name) {
        free(vigs_screen->drm->drm_render_node);
    }
    vigs_screen->drm->drm_render_node = NULL;

    drmFree(vigs_screen->drm->drm_device_name);
    vigs_screen->drm->drm_device_name = NULL;

    drmClose(vigs_screen->drm->fd);
    vigs_screen->drm->fd = -1;

    vigs_drm_queue_drop(vigs_screen->drm);

    free(vigs_screen->drm);
    vigs_screen->drm = NULL;
}

Bool vigs_drm_set_master(struct vigs_drm *drm)
{
    if (drmSetMaster(drm->fd) != 0) {
        xf86DrvMsg(drm->screen->scrn->scrnIndex, X_ERROR, "Unable to become DRM master\n");
        return FALSE;
    }

    return TRUE;
}

void vigs_drm_drop_master(struct vigs_drm *drm)
{
    if (drmDropMaster(drm->fd) != 0) {
        xf86DrvMsg(drm->screen->scrn->scrnIndex, X_ERROR, "Unable to drop DRM master\n");
    }
}

Bool vigs_drm_pageflip(struct vigs_drm *drm,
                       struct vigs_drm_surface *sfc,
                       void *data,
                       vigs_drm_handler_proc handler,
                       vigs_drm_abort_proc abort)
{
    ScrnInfoPtr scrn = drm->screen->scrn;
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(scrn);
    uint32_t new_fb_id;
    int ret;
    struct vigs_drm_pageflip_data *pageflip_data;
    int i;

    ret = drmModeAddFB(drm->fd,
                       sfc->width,
                       sfc->height,
                       scrn->depth,
                       scrn->bitsPerPixel,
                       sfc->stride,
                       sfc->gem.handle,
                       &new_fb_id);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to add FB: %s\n", strerror(-ret));
        return FALSE;
    }

    pageflip_data = calloc(1, sizeof(*pageflip_data));

    if (!pageflip_data) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate pageflip data\n");
        goto fail1;
    }

    pageflip_data->drm = drm;
    pageflip_data->data = data;
    pageflip_data->old_fb_id = drm->fb_id;
    pageflip_data->handler = handler;
    pageflip_data->abort = abort;

    for (i = 0; i < crtc_config->num_crtc; ++i) {
        xf86CrtcPtr crtc = crtc_config->crtc[i];
        struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;

        if (vigs_drm_crtc_is_on(crtc)) {
            ret = drmModePageFlip(drm->fd,
                                  vigs_crtc->mode_crtc->crtc_id,
                                  new_fb_id,
                                  DRM_MODE_PAGE_FLIP_EVENT,
                                  pageflip_data);

            if (ret < 0) {
                xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to page flip: %s\n", strerror(-ret));
                goto fail2;
            }

            drm->fb_id = new_fb_id;

            return TRUE;
        }
    }

    xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to page flip: no enabled CRTC\n");

fail2:
    free(pageflip_data);
fail1:
    drmModeRmFB(drm->fd, new_fb_id);

    return FALSE;
}
