#include "vigs_drm.h"
#include "vigs_drm_crtc.h"
#include "vigs_drm_output.h"
#include "vigs_screen.h"
#include "vigs_log.h"
#include "vigs_dri2.h"
#include "xf86Crtc.h"
#include "vigs.h"
#include <sys/poll.h>
#include <errno.h>

struct vigs_drm_pageflip_data
{
    struct vigs_drm *drm;
    struct vigs_dri2_frame_event *frame_event;
    uint32_t old_fb_id;
};

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
    vigs_dri2_vblank_handler((struct vigs_dri2_frame_event*)user_data,
                             sequence,
                             tv_sec,
                             tv_usec);
}

static void vigs_drm_page_flip_handler(int fd,
                                       unsigned int sequence,
                                       unsigned int tv_sec,
                                       unsigned int tv_usec,
                                       void *user_data)
{
    struct vigs_drm_pageflip_data *pageflip_data = user_data;
    struct vigs_drm *drm = pageflip_data->drm;
    struct vigs_dri2_frame_event *frame_event = pageflip_data->frame_event;
    uint32_t old_fb_id = pageflip_data->old_fb_id;

    free(pageflip_data);

    drmModeRmFB(drm->fd, old_fb_id);

    vigs_dri2_page_flip_handler(frame_event,
                                sequence,
                                tv_sec,
                                tv_usec);
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

    xf86CrtcSetSizeRange(scrn, 320, 200,
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

    if (!xf86InitialConfiguration(scrn, TRUE)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to perform initial configuration\n");
        return FALSE;
    }

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
    vigs_drm_device_destroy(vigs_screen->drm->dev);
    vigs_screen->drm->dev = NULL;

    drmFree(vigs_screen->drm->drm_device_name);
    vigs_screen->drm->drm_device_name = NULL;

    drmClose(vigs_screen->drm->fd);
    vigs_screen->drm->fd = -1;

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
                       struct vigs_dri2_frame_event *frame_event,
                       struct vigs_drm_surface *sfc)
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
    pageflip_data->frame_event = frame_event;
    pageflip_data->old_fb_id = drm->fb_id;

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
