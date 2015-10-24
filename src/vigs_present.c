#include "vigs_present.h"
#include "vigs_screen.h"
#include "vigs_drm.h"
#include "vigs_drm_crtc.h"
#include "vigs_pixmap.h"
#include "vigs_utils.h"
#include "vigs_uxa.h"
#include "vigs_log.h"
#include "vigs.h"
#include <errno.h>

struct vigs_present_vblank_event {
    uint64_t event_id;
};

static RRCrtcPtr vigs_present_get_crtc(WindowPtr window)
{
    ScreenPtr screen = window->drawable.pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    BoxRec box, crtcbox;
    xf86CrtcPtr crtc;
    RRCrtcPtr randr_crtc = NULL;

    VIGS_LOG_TRACE("window.drawable = 0x%X", window->drawable.id);

    box.x1 = window->drawable.x;
    box.y1 = window->drawable.y;
    box.x2 = box.x1 + window->drawable.width;
    box.y2 = box.y1 + window->drawable.height;

    crtc = vigs_screen_covering_crtc(scrn, &box, NULL, &crtcbox);

    /* Make sure the CRTC is valid and this is the real front buffer */
    if (crtc != NULL && !crtc->rotatedData)
            randr_crtc = crtc->randr_crtc;

    return randr_crtc;
}

static int vigs_present_get_ust_msc(RRCrtcPtr rrcrtc, CARD64 *ust, CARD64 *msc)
{
    xf86CrtcPtr crtc = rrcrtc->devPrivate;
    struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;
    struct vigs_screen *vigs_screen = vigs_crtc->drm->screen;
    ScrnInfoPtr scrn = vigs_screen->scrn;
    drmVBlank vbl;
    int ret;

    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 0;
    vbl.request.signal = 0;

    ret = drmWaitVBlank(vigs_crtc->drm->fd, &vbl);

    if (ret) {
        VIGS_LOG_ERROR("drmWaitVBlank failed: %s", strerror(errno));

        *ust = 0;
        *msc = 0;

        return BadMatch;
    }

    *ust = ((CARD64)vbl.reply.tval_sec * 1000000) + vbl.reply.tval_usec;
    *msc = vbl.reply.sequence;

    VIGS_LOG_TRACE("ust = %llu, msc = %llu", *ust, *msc);

    return Success;
}

/*
 * Called when the queued vblank event has occurred
 */
static void vigs_present_vblank_handler(void *data,
                                        unsigned int sequence,
                                        unsigned int tv_sec,
                                        unsigned int tv_usec)
{
    struct vigs_present_vblank_event *event = data;
    uint64_t usec = (uint64_t)tv_sec * 1000000 + tv_usec;
    uint64_t msc = sequence;

    VIGS_LOG_TRACE("event_id = %llu, usec = %llu, msc = %llu",
                   event->event_id,
                   usec,
                   msc);

    present_event_notify(event->event_id, usec, msc);

    free(event);
}

/*
 * Called when the queued vblank is aborted
 */
static void vigs_present_vblank_abort(void *data)
{
    struct vigs_present_vblank_event *event = data;

    VIGS_LOG_TRACE("event_id = %llu", event->event_id);

    free(event);
}

static Bool vigs_present_flush_drm_events(struct vigs_screen *vigs_screen)
{
    return vigs_drm_read_events(vigs_screen->drm) >= 0;
}

static int vigs_present_queue_vblank(RRCrtcPtr rrcrtc,
                                     uint64_t event_id,
                                     uint64_t msc)
{
    xf86CrtcPtr crtc = rrcrtc->devPrivate;
    struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;
    struct vigs_screen *vigs_screen = vigs_crtc->drm->screen;
    ScrnInfoPtr scrn = vigs_screen->scrn;
    struct vigs_present_vblank_event *event;
    struct vigs_drm_queue *q;
    drmVBlank vbl;
    int ret;

    VIGS_LOG_TRACE("event_id = %lu, msc = %llu", event_id, msc);

    event = calloc(1, sizeof(*event));

    if (!event) {
        return BadAlloc;
    }

    event->event_id = event_id;

    q = vigs_drm_queue_alloc(vigs_screen->drm,
                             event,
                             vigs_present_vblank_handler,
                             vigs_present_vblank_abort);

    if (!q) {
        free(event);
        return BadAlloc;
    }

    vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
    vbl.request.sequence = msc;
    vbl.request.signal = (unsigned long)q;

    for (;;) {
        ret = drmWaitVBlank(vigs_screen->drm->fd, &vbl);

        if (!ret) {
            break;
        }

        if (errno != EBUSY || !vigs_present_flush_drm_events(vigs_screen)) {
            VIGS_LOG_ERROR("drmWaitVBlank failed");
            return BadAlloc;
        }

    }

    return Success;
}

static Bool vigs_present_event_match(void *data, void *match_data)
{
    struct vigs_present_vblank_event *event = data;
    uint64_t *match = match_data;

    return (*match == event->event_id);
}

static void vigs_present_abort_vblank(RRCrtcPtr rrcrtc,
                                      uint64_t event_id,
                                      uint64_t msc)
{
    xf86CrtcPtr crtc = rrcrtc->devPrivate;
    struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;

    VIGS_LOG_TRACE("event_id = %llu, msc = %llu", event_id, msc);

    vigs_drm_queue_abort(vigs_crtc->drm, vigs_present_event_match, &event_id);
}

static void vigs_present_flush(WindowPtr window)
{
    ScreenPtr screen = window->drawable.pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;

    VIGS_LOG_TRACE("window.drawable = 0x%X", window->drawable.id);

    vigs_uxa_flush(vigs_screen);
}

static Bool vigs_present_check_flip(RRCrtcPtr rrcrtc,
                                    WindowPtr window,
                                    PixmapPtr pixmap,
                                    Bool sync_flip)
{
    ScreenPtr screen = window->drawable.pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    VIGS_LOG_TRACE("enter");

    if (!scrn->vtSema) {
        return FALSE;
    }

    if (!vigs_screen->pageflip) {
        return FALSE;
    }

    if (rrcrtc && !vigs_drm_crtc_is_on(rrcrtc->devPrivate)) {
        return FALSE;
    }

    if (vigs_pixmap_stride(pixmap) != vigs_screen->front_sfc->stride) {
        return FALSE;
    }

    if (!vigs_pixmap || !vigs_pixmap->sfc) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Once the flip has been completed on all pipes, notify the
 * extension code telling it when that happened
 */
static void vigs_present_flip_handler(void *data,
                                      unsigned int sequence,
                                      unsigned int tv_sec,
                                      unsigned int tv_usec)
{
    struct vigs_present_vblank_event *event = data;
    uint64_t usec = (uint64_t)tv_sec * 1000000 + tv_usec;
    uint64_t msc = sequence;

    VIGS_LOG_TRACE("event_id = %llu, usec = %llu, msc = %llu",
                   event->event_id,
                   usec,
                   msc);

    present_event_notify(event->event_id, usec, msc);

    free(event);
}

/*
 * The flip has been aborted, free the structure
 */
static void vigs_present_flip_abort(void *data)
{
    struct vigs_present_vblank_event *event = data;

    VIGS_LOG_TRACE("event_id = %llu", event->event_id);

    free(event);
}

static Bool vigs_present_flip(RRCrtcPtr rrcrtc,
                              uint64_t event_id,
                              uint64_t target_msc,
                              PixmapPtr pixmap,
                              Bool sync_flip)
{
    ScreenPtr screen = rrcrtc->pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    struct vigs_present_vblank_event *event;

    VIGS_LOG_TRACE("event_id = %llu, target_msc = %llu, pixmap = 0x%X, sync_flip = %d",
                   event_id,
                   target_msc,
                   pixmap->drawable.id,
                   sync_flip);

    if (!vigs_present_check_flip(rrcrtc, screen->root, pixmap, sync_flip)) {
        return FALSE;
    }

    event = calloc(1, sizeof(*event));

    if (!event) {
        return FALSE;
    }

    event->event_id = event_id;

    /* TODO handle sync_flip */

    return vigs_drm_pageflip(vigs_screen->drm,
                             vigs_pixmap->sfc,
                             event,
                             vigs_present_flip_handler,
                             vigs_present_flip_abort);
}

static void vigs_present_unflip(ScreenPtr screen, uint64_t event_id)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    PixmapPtr pixmap = screen->GetScreenPixmap(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    struct vigs_present_vblank_event *event = NULL;

    VIGS_LOG_TRACE("event_id = %llu", event_id);

    if (!vigs_present_check_flip(NULL, screen->root, pixmap, true)) {
        goto fail;
    }

    event = calloc(1, sizeof(*event));

    if (!event) {
        goto fail;
    }

    event->event_id = event_id;

    if (!vigs_drm_pageflip(vigs_screen->drm,
                           vigs_pixmap->sfc,
                           event,
                           vigs_present_flip_handler,
                           vigs_present_flip_abort)) {
        goto fail;
    }

    return;

fail:
    present_event_notify(event_id, 0, 0);
    free(event);
}

static present_screen_info_rec vigs_present_info = {
    .version = PRESENT_SCREEN_INFO_VERSION,

    .get_crtc = vigs_present_get_crtc,
    .get_ust_msc = vigs_present_get_ust_msc,
    .queue_vblank = vigs_present_queue_vblank,
    .abort_vblank = vigs_present_abort_vblank,
    .flush = vigs_present_flush,

    .capabilities = PresentCapabilityNone,
    .check_flip = vigs_present_check_flip,
    .flip = vigs_present_flip,
    .unflip = vigs_present_unflip,
};

static Bool vigs_present_has_async_flip(ScreenPtr screen)
{
#ifdef DRM_CAP_ASYNC_PAGE_FLIP
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    int ret;
    uint64_t value;

    ret = drmGetCap(vigs_screen->drm->fd, DRM_CAP_ASYNC_PAGE_FLIP, &value);

    if (ret == 0) {
        return value == 1;
    }
#endif

    return FALSE;
}

Bool vigs_present_init(struct vigs_screen *vigs_screen)
{
    ScreenPtr screen = vigs_screen->scrn->pScreen;

    VIGS_LOG_TRACE("enter");

    if (vigs_present_has_async_flip(screen)) {
        vigs_present_info.capabilities |= PresentCapabilityAsync;
    }

    return present_screen_init(screen, &vigs_present_info);
}

void vigs_present_close(struct vigs_screen *vigs_screen)
{
    VIGS_LOG_TRACE("enter");
}
