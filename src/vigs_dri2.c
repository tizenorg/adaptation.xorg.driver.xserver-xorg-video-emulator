#include "vigs_dri2.h"
#include "vigs_screen.h"
#include "vigs_drm.h"
#include "vigs_pixmap.h"
#include "vigs_log.h"
#include "vigs_uxa.h"
#include "vigs_utils.h"
#include "vigs_drm_crtc.h"
#include "vigs.h"
#include <errno.h>

struct vigs_dri2_buffer
{
    DRI2BufferRec base;
    PixmapPtr pixmap;

    /*
     * We need this in order not to destroy the buffer when
     * a swap/flip is pending.
     */
    int refcnt;
};

struct vigs_dri2_client
{
    struct vigs_list frame_events;
};

#if HAS_DEVPRIVATEKEYREC
static DevPrivateKeyRec vigs_dri2_client_index;
#else
static int vigs_dri2_client_index;
#endif

static inline struct vigs_dri2_client *client_to_vigs_dri2_client(ClientPtr client)
{
#if HAS_DIXREGISTERPRIVATEKEY
    return dixGetPrivateAddr(&client->devPrivates, &vigs_dri2_client_index);
#else
    return dixLookupPrivate(&client->devPrivates, &vigs_dri2_client_index);
#endif
}

static Bool vigs_dri2_client_subsystem_init(struct vigs_screen *vigs_screen)
{
#if HAS_DIXREGISTERPRIVATEKEY
    if (!dixRegisterPrivateKey(&vigs_dri2_client_index, PRIVATE_CLIENT,
                               sizeof(struct vigs_dri2_client))) {
#else
    if (!dixRequestPrivate(&vigs_dri2_client_index,
                           sizeof(struct vigs_dri2_client))) {
#endif
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to request client private\n");
        return FALSE;
    }

    return TRUE;
}

static void vigs_dri2_send_sync_draw_done(ScreenPtr screen,
                                          ClientPtr client,
                                          DrawablePtr drawable)
{
    static Atom sync_draw_done = None;
    xEvent event;
    DeviceIntPtr dev = PickPointer(client);

    if (sync_draw_done == None) {
        sync_draw_done = MakeAtom("_E_COMP_SYNC_DRAW_DONE",
                                  strlen("_E_COMP_SYNC_DRAW_DONE"),
                                  TRUE);
    }

    memset (&event, 0, sizeof (xEvent));
    event.u.u.type = ClientMessage;
    event.u.u.detail = 32;
    event.u.clientMessage.u.l.type = sync_draw_done;
    event.u.clientMessage.u.l.longs0 = drawable->id; // window id
    event.u.clientMessage.u.l.longs1 = 1; // version
    event.u.clientMessage.u.l.longs2 = drawable->width; // window's width
    event.u.clientMessage.u.l.longs3 = drawable->height; // window's height

    VIGS_LOG_TRACE("client=%d pDraw->id=%x width=%d height=%d\n",
                   client->index, drawable->id,
                   drawable->width, drawable->height);

    DeliverEventsToWindow(dev, screen->root, &event, 1,
                          SubstructureRedirectMask | SubstructureNotifyMask,
                          NullGrab);
}

static DRI2BufferPtr vigs_dri2_create_buffer(DrawablePtr drawable,
                                             unsigned int attachment,
                                             unsigned int format)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(drawable->pScreen);
    int depth;
    PixmapPtr pixmap = NULL;
    struct vigs_pixmap *vigs_pixmap = NULL;
    struct vigs_dri2_buffer *buffer = NULL;

    VIGS_LOG_TRACE("drawable = %p, attachment = %u, format = %u",
                   drawable,
                   attachment,
                   format);

    switch (attachment) {
    case DRI2BufferFrontLeft:
        pixmap = vigs_get_drawable_pixmap(drawable);

        if (pixmap) {
            ++pixmap->refcnt;
        }

        break;
    case DRI2BufferBackLeft:
    case DRI2BufferFakeFrontLeft:
        depth = (format != 0) ? format : drawable->depth;

        if ((depth != 24) && (depth != 32)) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Only 24 and 32 depth buffers are supported for now\n");
            goto fail1;
        }

        pixmap = drawable->pScreen->CreatePixmap(
            drawable->pScreen,
            drawable->width,
            drawable->height,
            depth,
            0);

        if (!pixmap) {
            goto fail1;
        }

        break;
    case DRI2BufferFrontRight:
    case DRI2BufferBackRight:
    case DRI2BufferDepth:
    case DRI2BufferStencil:
    case DRI2BufferAccum:
    case DRI2BufferFakeFrontRight:
    case DRI2BufferDepthStencil:
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Attachment %u not supported\n", attachment);
        goto fail1;
    default:
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unknown attachment %u\n", attachment);
        goto fail1;
    }

    if (pixmap) {
        vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

        if (!vigs_pixmap ||
            !vigs_pixmap_create_sfc(pixmap) ||
            !vigs_pixmap_get_name(pixmap)) {
            goto fail2;
        }
    }

    buffer = calloc(1, sizeof(*buffer));

    if (!buffer) {
        goto fail2;
    }

    if (pixmap) {
        buffer->base.name = vigs_pixmap->sfc->gem.name;
        buffer->base.pitch = vigs_pixmap_stride(pixmap);
        buffer->base.cpp = vigs_pixmap_bpp(pixmap);
    }

    buffer->base.attachment = attachment;
    buffer->base.flags = 0;
    buffer->base.format = format;
    buffer->base.driverPrivate = buffer;
    buffer->pixmap = pixmap;
    buffer->refcnt = 1;

    VIGS_LOG_TRACE("buffer created = %p", &buffer->base);

    return &buffer->base;

fail2:
    if (pixmap) {
        drawable->pScreen->DestroyPixmap(pixmap);
    }
fail1:

    return NULL;
}

static void vigs_dri2_destroy_buffer(DrawablePtr drawable,
                                     DRI2BufferPtr buffer)
{
    struct vigs_dri2_buffer *vigs_buffer = (struct vigs_dri2_buffer*)buffer;

    if (!vigs_buffer) {
        return;
    }

    if (--vigs_buffer->refcnt > 0) {
        return;
    }

    VIGS_LOG_TRACE("drawable = %p, buffer = %p", drawable, buffer);

    drawable->pScreen->DestroyPixmap(vigs_buffer->pixmap);

    free(vigs_buffer);
}

static void vigs_dri2_ref_buffer(DRI2BufferPtr buffer)
{
    struct vigs_dri2_buffer *vigs_buffer = (struct vigs_dri2_buffer*)buffer;

    if (!vigs_buffer) {
        return;
    }

    ++vigs_buffer->refcnt;
}

static void vigs_dri2_unref_buffer(DRI2BufferPtr buffer)
{
    struct vigs_dri2_buffer *vigs_buffer = (struct vigs_dri2_buffer*)buffer;

    if (vigs_buffer) {
        vigs_dri2_destroy_buffer(&vigs_buffer->pixmap->drawable, buffer);
    }
}

static Bool vigs_dri2_client_add_frame_event(ClientPtr client,
                                             struct vigs_dri2_frame_event *frame_event)
{
    struct vigs_dri2_client
        *vigs_client = client_to_vigs_dri2_client(client);

    if (!vigs_client) {
        return FALSE;
    }

    vigs_list_add_tail(&vigs_client->frame_events, &frame_event->list);

    return TRUE;
}

static void vigs_dri2_client_remove_frame_event(struct vigs_dri2_frame_event *frame_event)
{
    vigs_list_remove(&frame_event->list);
    frame_event->client = NULL;
    frame_event->drawable_id = 0;
    frame_event->event_func = NULL;
    frame_event->event_data = NULL;
    vigs_dri2_unref_buffer(frame_event->src);
    frame_event->src = NULL;
    vigs_dri2_unref_buffer(frame_event->dest);
    frame_event->dest = NULL;
}

static void vigs_dri2_client_state_changed(CallbackListPtr *callback_list,
                                           pointer data,
                                           pointer call_data)
{
    NewClientInfoRec *client_info = call_data;
    ClientPtr client = client_info->client;
    struct vigs_dri2_client
        *vigs_client = client_to_vigs_dri2_client(client);

    switch (client->clientState) {
    case ClientStateInitial:
        VIGS_LOG_TRACE("client %p init", client);
        vigs_list_init(&vigs_client->frame_events);
        break;
    case ClientStateRunning:
        break;
    case ClientStateRetained:
    case ClientStateGone:
        VIGS_LOG_TRACE("client %p gone", client);
        if (vigs_client) {
            struct vigs_dri2_frame_event *frame_event, *tmp;

            vigs_list_for_each_safe(struct vigs_dri2_frame_event,
                                    frame_event,
                                    tmp,
                                    &vigs_client->frame_events,
                                    list) {
                vigs_dri2_client_remove_frame_event(frame_event);
            }
        }
        break;
    default:
        break;
    }
}

static Bool vigs_dri2_drawable_belongs_to_crtc(DrawablePtr drawable)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(drawable->pScreen);
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(scrn);
    xf86CrtcPtr crtc;
    int i;

    for (i = 0; i < crtc_config->num_crtc; ++i) {
        crtc = crtc_config->crtc[i];

        if (vigs_drm_crtc_is_on(crtc)) {
            return TRUE;
        }
    }

    return FALSE;
}

static Bool vigs_dri2_can_flip(DrawablePtr drawable,
                               DRI2BufferPtr dest,
                               DRI2BufferPtr src)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(drawable->pScreen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_dri2_buffer *vigs_dest = (struct vigs_dri2_buffer*)dest;
    struct vigs_dri2_buffer *vigs_src = (struct vigs_dri2_buffer*)src;

    if (!vigs_screen->pageflip) {
        return FALSE;
    }

    /*
     * Is this really needed ?
     */
    if (!scrn->vtSema) {
        return FALSE;
    }

    if (!vigs_dri2_drawable_belongs_to_crtc(drawable)) {
        return FALSE;
    }

    /*
     * This will check if 'drawable' is a root pixmap. Only root pixmaps
     * can be flipped, since root pixmap's surface is the one that's attached
     * to CRTC via DRM framebuffer.
     */
    if (!DRI2CanFlip(drawable)) {
        return FALSE;
    }

    /*
     * Check if formats match.
     */

    if (vigs_dest->pixmap->drawable.width != vigs_src->pixmap->drawable.width) {
        return FALSE;
    }

    if (vigs_dest->pixmap->drawable.height != vigs_src->pixmap->drawable.height) {
        return FALSE;
    }

    if (vigs_dest->pixmap->drawable.bitsPerPixel != vigs_src->pixmap->drawable.bitsPerPixel) {
        return FALSE;
    }

    return TRUE;
}

static void vigs_dri2_exchange_pixmaps(struct vigs_screen *vigs_screen,
                                       PixmapPtr dest,
                                       PixmapPtr src)
{
    struct vigs_pixmap *new_dest, *new_src;
    RegionRec region;

    /*
     * This is from intel driver:
     *
     * "Post damage on the front buffer so that listeners, such
     * as DisplayLink know take a copy and shove it over the USB.
     * also for sw cursors"
     *
     * Will leave it as is, not sure what it does exactly, but sending
     * damage can't possibly break anything.
     */
    region.extents.x1 = region.extents.y1 = 0;
    region.extents.x2 = dest->drawable.width;
    region.extents.y2 = dest->drawable.height;
    region.data = NULL;
    DamageRegionAppend(&dest->drawable, &region);

    new_dest = pixmap_to_vigs_pixmap(src);
    new_src = pixmap_to_vigs_pixmap(dest);
    vigs_pixmap_set_private(dest, new_dest);
    vigs_pixmap_set_private(src, new_src);

    DamageRegionProcessPending(&dest->drawable);
}

static void vigs_dri2_exchange_buffers(struct vigs_screen *vigs_screen,
                                       DRI2BufferPtr dest,
                                       DRI2BufferPtr src)
{
    struct vigs_dri2_buffer *vigs_dest = (struct vigs_dri2_buffer*)dest;
    struct vigs_dri2_buffer *vigs_src = (struct vigs_dri2_buffer*)src;
    uint32_t tmp;

    /*
     * Swap buffer names.
     */
    tmp = dest->name;
    dest->name = src->name;
    src->name = tmp;

    /*
     * Swap pixmaps.
     */
    vigs_dri2_exchange_pixmaps(vigs_screen,
                               vigs_dest->pixmap,
                               vigs_src->pixmap);

    if (vigs_screen->front_sfc == pixmap_to_vigs_pixmap(vigs_src->pixmap)->sfc) {
        vigs_drm_gem_unref(&vigs_screen->front_sfc->gem);
        vigs_screen->front_sfc = pixmap_to_vigs_pixmap(vigs_dest->pixmap)->sfc;
        vigs_drm_gem_ref(&vigs_screen->front_sfc->gem);
    }
}

static Bool vigs_dri2_schedule_flip(DrawablePtr drawable,
                                    struct vigs_dri2_frame_event *frame_event)
{
    struct vigs_screen *vigs_screen = frame_event->screen;
    struct vigs_dri2_buffer *vigs_src = (struct vigs_dri2_buffer*)frame_event->src;
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(vigs_src->pixmap);

    VIGS_LOG_TRACE("drawable = 0x%X", drawable->id);

    frame_event->type = vigs_dri2_swap;

    if (!vigs_drm_pageflip(vigs_screen->drm,
                           frame_event,
                           vigs_pixmap->sfc)) {
        return FALSE;
    }

    vigs_dri2_exchange_buffers(vigs_screen,
                               frame_event->dest,
                               frame_event->src);

    return TRUE;
}

static void vigs_dri2_copy_region(DrawablePtr drawable,
                                  RegionPtr region,
                                  DRI2BufferPtr dest,
                                  DRI2BufferPtr src)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(drawable->pScreen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_dri2_buffer *vigs_dst = (struct vigs_dri2_buffer*)dest;
    struct vigs_dri2_buffer *vigs_src = (struct vigs_dri2_buffer*)src;
    RegionPtr copy_clip;
    GCPtr gc;
    DrawablePtr src_draw, dst_draw;
    struct vigs_pixmap *vigs_pixmap;

    VIGS_LOG_TRACE("dest = %p, src = %p", dest, src);

    src_draw = &vigs_src->pixmap->drawable;
    dst_draw = &vigs_dst->pixmap->drawable;

    if (vigs_src->base.attachment == DRI2BufferFrontLeft) {
        src_draw = drawable;
    }

    if (vigs_dst->base.attachment == DRI2BufferFrontLeft) {
        dst_draw = drawable;
    }

    if (vigs_src->pixmap == vigs_dst->pixmap) {
        /*
         * Pixmap case.
         */
        if ((src->attachment == DRI2BufferFrontLeft) &&
            (dest->attachment == DRI2BufferFakeFrontLeft)) {
            /*
             * eglWaitNative.
             */
            vigs_uxa_flush(vigs_screen);
        } else if ((src->attachment == DRI2BufferFakeFrontLeft) &&
                   (dest->attachment == DRI2BufferFrontLeft)) {
            /*
             * eglWaitClient.
             */
        } else {
            vigs_uxa_flush(vigs_screen);
        }
        return;
    }

    gc = GetScratchGC(drawable->depth, drawable->pScreen);
    if (!gc) {
        return;
    }

    copy_clip = REGION_CREATE(drawable->pScreen, NULL, 0);

    REGION_COPY(drawable->pScreen, copy_clip, region);

    gc->funcs->ChangeClip(gc, CT_REGION, copy_clip, 0);

    ValidateGC(dst_draw, gc);

    if (dest->attachment == DRI2BufferFrontLeft) {
        vigs_pixmap = pixmap_to_vigs_pixmap(vigs_dst->pixmap);

        vigs_drm_gem_wait(&vigs_pixmap->sfc->gem);
    }

    gc->ops->CopyArea(src_draw, dst_draw, gc, 0, 0,
                      drawable->width, drawable->height,
                      0, 0);

    FreeScratchGC(gc);
}

static void vigs_dri2_blit_swap(DrawablePtr drawable,
                                DRI2BufferPtr dest,
                                DRI2BufferPtr src)
{
    BoxRec box;
    RegionRec region;

    box.x1 = 0;
    box.y1 = 0;
    box.x2 = drawable->width;
    box.y2 = drawable->height;

    REGION_INIT(drawable->pScreen, &region, &box, 0);

    vigs_dri2_copy_region(drawable, &region, dest, src);
}

/*
 * Heavily based on intel's I830DRI2FrameEventHandler.
 */
void vigs_dri2_vblank_handler(struct vigs_dri2_frame_event *frame_event,
                              unsigned int sequence,
                              unsigned int tv_sec,
                              unsigned int tv_usec)
{
    DrawablePtr drawable;
    int ret;
    ScrnInfoPtr scrn;

    if (!frame_event->client) {
        /*
         * Client gone, event is invalid.
         */
        VIGS_LOG_TRACE("client gone");

        goto out;
    }

    VIGS_LOG_TRACE("client = %p, drawable = 0x%X, seq = %u, tv_sec = %u, tv_usec = %u",
                   frame_event->client,
                   frame_event->drawable_id,
                   sequence,
                   tv_sec,
                   tv_usec);

    ret = dixLookupDrawable(&drawable,
                            frame_event->drawable_id,
                            serverClient,
                            M_ANY,
                            DixWriteAccess);

    if (ret != Success) {
        /*
         * Drawable gone.
         */
        VIGS_LOG_TRACE("drawable 0x%X gone", frame_event->drawable_id);

        goto out;
    }

    scrn = xf86ScreenToScrn(drawable->pScreen);

    switch (frame_event->type) {
    case vigs_dri2_flip:
        /*
         * If we can still flip...
         */
        if (vigs_dri2_can_flip(drawable, frame_event->dest, frame_event->src) &&
            vigs_dri2_schedule_flip(drawable, frame_event)) {
            return;
        }
        /*
         * Else fall through to swap.
         */
    case vigs_dri2_swap:
        vigs_dri2_blit_swap(drawable, frame_event->dest, frame_event->src);

        DRI2SwapComplete(frame_event->client,
                         drawable, sequence, tv_sec, tv_usec,
                         DRI2_BLIT_COMPLETE,
                         frame_event->event_func,
                         frame_event->event_data);
        vigs_dri2_send_sync_draw_done(drawable->pScreen,
                                      frame_event->client,
                                      drawable);
        break;
    case vigs_dri2_waitmsc:
        DRI2WaitMSCComplete(frame_event->client,
                            drawable,
                            sequence, tv_sec, tv_usec);
        break;
    default:
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unknown vblank event received\n");
        break;
    }

out:
    vigs_dri2_client_remove_frame_event(frame_event);

    free(frame_event);
}

/*
 * Heavily based on intel's I830DRI2FlipEventHandler.
 */
void vigs_dri2_page_flip_handler(struct vigs_dri2_frame_event *frame_event,
                                 unsigned int sequence,
                                 unsigned int tv_sec,
                                 unsigned int tv_usec)
{
    DrawablePtr drawable;
    int ret;
    ScrnInfoPtr scrn;

    if (!frame_event->client) {
        /*
         * Client gone, event is invalid.
         */
        VIGS_LOG_TRACE("client gone");

        goto out;
    }

    VIGS_LOG_TRACE("client = %p, drawable = 0x%X, seq = %u, tv_sec = %u, tv_usec = %u",
                   frame_event->client,
                   frame_event->drawable_id,
                   sequence,
                   tv_sec,
                   tv_usec);

    ret = dixLookupDrawable(&drawable,
                            frame_event->drawable_id,
                            serverClient,
                            M_ANY,
                            DixWriteAccess);

    if (ret != Success) {
        /*
         * Drawable gone.
         */
        VIGS_LOG_TRACE("drawable 0x%X gone", frame_event->drawable_id);

        goto out;
    }

    scrn = xf86ScreenToScrn(drawable->pScreen);

    /*
     * We assume our flips arrive in order, so we don't check the frame.
     */
    switch (frame_event->type) {
    case vigs_dri2_swap:
        /*
         * Check for too small vblank count of pageflip completion, taking wraparound
         * into account. This usually means some defective kms pageflip completion,
         * causing wrong (msc, ust) return values and possible visual corruption.
         */
        if ((sequence < frame_event->sequence) &&
            (frame_event->sequence - sequence < 5)) {
            xf86DrvMsg(scrn->scrnIndex, X_WARNING, "Pageflip completion has impossible msc %d < target_msc %d\n",
                       sequence, frame_event->sequence);

            sequence = tv_sec = tv_usec = 0;
        }

        DRI2SwapComplete(frame_event->client,
                         drawable, sequence, tv_sec, tv_usec,
                         DRI2_FLIP_COMPLETE,
                         frame_event->event_func,
                         frame_event->event_data);
        vigs_dri2_send_sync_draw_done(drawable->pScreen,
                                      frame_event->client,
                                      drawable);
        break;
    default:
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unknown pageflip event received\n");
        break;
    }

out:
    vigs_dri2_client_remove_frame_event(frame_event);

    free(frame_event);
}

/*
 * Heavily based on intel's I830DRI2ScheduleSwap.
 */
static int vigs_dri2_schedule_swap(ClientPtr client,
                                   DrawablePtr drawable,
                                   DRI2BufferPtr dest,
                                   DRI2BufferPtr src,
                                   CARD64 *target_msc,
                                   CARD64 divisor,
                                   CARD64 remainder,
                                   DRI2SwapEventPtr func,
                                   void *data)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(drawable->pScreen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_dri2_frame_event *frame_event = NULL;
    drmVBlank vbl;
    int ret;
    CARD64 current_msc;
    int flip = 0;

    /*
     * Truncate to match kernel interfaces; means occasional overflow
     * misses, but that's generally not a big deal.
     */
    *target_msc &= 0xffffffff;
    divisor &= 0xffffffff;
    remainder &= 0xffffffff;

    if (!vigs_screen->vsync) {
        goto fallback;
    }

    VIGS_LOG_TRACE("client = %p, drawable = 0x%X, target_msc = %u, divisor = %u, remainder = %u",
                   client,
                   drawable->id,
                   (uint32_t)*target_msc,
                   (uint32_t)divisor,
                   (uint32_t)remainder);

    if (!vigs_dri2_drawable_belongs_to_crtc(drawable)) {
        VIGS_LOG_TRACE("drawable 0x%X doesn't belong to CRTC", drawable->id);
        goto fallback;
    }

    frame_event = calloc(1, sizeof(*frame_event));

    if (!frame_event) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Cannot allocate frame event\n");
        goto fallback;
    }

    frame_event->screen = vigs_screen;
    vigs_list_init(&frame_event->list);
    frame_event->client = client;
    frame_event->drawable_id = drawable->id;
    frame_event->type = vigs_dri2_swap;
    frame_event->event_func = func;
    frame_event->event_data = data;
    frame_event->src = src;
    frame_event->dest = dest;

    if (!vigs_dri2_client_add_frame_event(client, frame_event)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Cannot add frame event to client\n");
        free(frame_event);
        frame_event = NULL;
        goto fallback;
    }

    vigs_dri2_ref_buffer(src);
    vigs_dri2_ref_buffer(dest);

    /*
     * Get current count.
     */
    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 0;

    ret = drmWaitVBlank(vigs_screen->drm->fd, &vbl);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_WARNING, "drmWaitVBlank failed: %s\n", strerror(errno));
        goto fallback;
    }

    current_msc = vbl.reply.sequence;

    /*
     * Flips need to be submitted one frame before.
     */
    if (vigs_dri2_can_flip(drawable, dest, src)) {
        frame_event->type = vigs_dri2_flip;
        flip = 1;
    }

    /*
     * Correct target_msc by 'flip'.
     * Do it early, so handling of different timing constraints
     * for divisor, remainder and msc vs. target_msc works.
     */
    if (*target_msc > 0) {
        *target_msc -= flip;
    }

    /*
     * If divisor is zero, or current_msc is smaller than target_msc
     * we just need to make sure target_msc passes before initiating
     * the swap.
     */
    if ((divisor == 0) || (current_msc < *target_msc)) {
        /*
         * If we can, schedule the flip directly from here rather
         * than waiting for an event from the kernel for the current
         * (or a past) MSC.
         */
        if (flip &&
            (divisor == 0) &&
            (current_msc >= *target_msc) &&
            vigs_dri2_schedule_flip(drawable, frame_event)) {
            return TRUE;
        }

        vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;

        /*
         * If non-pageflipping, but blitting/exchanging, we need to use
         * DRM_VBLANK_NEXTONMISS to avoid unreliable timestamping later
         * on.
         */
        if (flip == 0) {
            vbl.request.type |= DRM_VBLANK_NEXTONMISS;
        }

        /*
         * If target_msc already reached or passed, set it to
         * current_msc to ensure we return a reasonable value back
         * to the caller. This makes swap_interval logic more robust.
         */
        if (current_msc >= *target_msc) {
            *target_msc = current_msc;
        }

        vbl.request.sequence = *target_msc;
        vbl.request.signal = (unsigned long)frame_event;

        ret = drmWaitVBlank(vigs_screen->drm->fd, &vbl);

        if (ret != 0) {
            xf86DrvMsg(scrn->scrnIndex, X_WARNING, "drmWaitVBlank failed: %s\n", strerror(errno));
            goto fallback;
        }

        *target_msc = vbl.reply.sequence + flip;
        frame_event->sequence = *target_msc;

        return TRUE;
    }

    /*
     * If we get here, target_msc has already passed or we don't have one,
     * and we need to queue an event that will satisfy the divisor/remainder
     * equation.
     */
    vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;

    if (flip == 0) {
        vbl.request.type |= DRM_VBLANK_NEXTONMISS;
    }

    vbl.request.sequence = current_msc - (current_msc % divisor) + remainder;

    /*
     * If the calculated deadline vbl.request.sequence is smaller than
     * or equal to current_msc, it means we've passed the last point
     * when effective onset frame seq could satisfy
     * seq % divisor == remainder, so we need to wait for the next time
     * this will happen.

     * This comparison takes the 1 frame swap delay in pageflipping mode
     * into account, as well as a potential DRM_VBLANK_NEXTONMISS delay
     * if we are blitting/exchanging instead of flipping.
     */
    if (vbl.request.sequence <= current_msc) {
        vbl.request.sequence += divisor;
    }

    /*
     * Account for 1 frame extra pageflip delay if flip > 0.
     */
    vbl.request.sequence -= flip;
    vbl.request.signal = (unsigned long)frame_event;

    ret = drmWaitVBlank(vigs_screen->drm->fd, &vbl);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_WARNING, "drmWaitVBlank failed: %s\n", strerror(errno));
        goto fallback;
    }

    /*
     * Adjust returned value for 1 fame pageflip offset of flip > 0.
     */
    *target_msc = vbl.reply.sequence + flip;
    frame_event->sequence = *target_msc;

    return TRUE;

fallback:
    vigs_dri2_blit_swap(drawable, dest, src);

    DRI2SwapComplete(client, drawable, 0, 0, 0, DRI2_BLIT_COMPLETE, func, data);

    vigs_dri2_send_sync_draw_done(drawable->pScreen,
                                  client,
                                  drawable);

    if (frame_event) {
        vigs_dri2_client_remove_frame_event(frame_event);
        free(frame_event);
    }

    *target_msc = 0;

    return TRUE;
}

/*
 * Heavily based on intel's I830DRI2GetMSC.
 */
static int vigs_dri2_get_msc(DrawablePtr drawable,
                             CARD64 *ust,
                             CARD64 *msc)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(drawable->pScreen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    drmVBlank vbl;
    int ret;

    if (!vigs_screen->vsync) {
        *ust = vigs_gettime_us();
        *msc = 0;
        return TRUE;
    }

    VIGS_LOG_TRACE("drawable = 0x%X", drawable->id);

    if (!vigs_dri2_drawable_belongs_to_crtc(drawable)) {
        VIGS_LOG_TRACE("drawable 0x%X doesn't belong to CRTC", drawable->id);
        *ust = vigs_gettime_us();
        *msc = 0;
        return TRUE;
    }

    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 0;

    ret = drmWaitVBlank(vigs_screen->drm->fd, &vbl);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_WARNING, "drmWaitVBlank failed: %s\n", strerror(errno));
        return FALSE;
    }

    *ust = ((CARD64)vbl.reply.tval_sec * 1000000) + vbl.reply.tval_usec;
    *msc = vbl.reply.sequence;

    return TRUE;
}

/*
 * Heavily based on intel's I830DRI2ScheduleWaitMSC.
 */
static int vigs_dri2_schedule_wait_msc(ClientPtr client,
                                       DrawablePtr drawable,
                                       CARD64 target_msc,
                                       CARD64 divisor,
                                       CARD64 remainder)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(drawable->pScreen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_dri2_frame_event *frame_event = NULL;
    drmVBlank vbl;
    int ret;
    CARD64 current_msc;

    target_msc &= 0xffffffff;
    divisor &= 0xffffffff;
    remainder &= 0xffffffff;

    if (!vigs_screen->vsync) {
        goto complete;
    }

    VIGS_LOG_TRACE("client = %p, drawable = 0x%X, target_msc = %u, divisor = %u, remainder = %u",
                   client,
                   drawable->id,
                   (uint32_t)target_msc,
                   (uint32_t)divisor,
                   (uint32_t)remainder);

    if (!vigs_dri2_drawable_belongs_to_crtc(drawable)) {
        VIGS_LOG_TRACE("drawable 0x%X doesn't belong to CRTC", drawable->id);
        goto complete;
    }

    frame_event = calloc(1, sizeof(*frame_event));

    if (!frame_event) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Cannot allocate frame event\n");
        goto complete;
    }

    frame_event->screen = vigs_screen;
    vigs_list_init(&frame_event->list);
    frame_event->client = client;
    frame_event->drawable_id = drawable->id;
    frame_event->type = vigs_dri2_waitmsc;

    if (!vigs_dri2_client_add_frame_event(client, frame_event)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Cannot add frame event to client\n");
        free(frame_event);
        frame_event = NULL;
        goto complete;
    }

    /*
     * Get current count.
     */
    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 0;

    ret = drmWaitVBlank(vigs_screen->drm->fd, &vbl);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_WARNING, "drmWaitVBlank failed: %s\n", strerror(errno));
        goto complete;
    }

    current_msc = vbl.reply.sequence;

    /*
     * If divisor is zero, or current_msc is smaller than target_msc,
     * we just need to make sure target_msc passes  before waking up the
     * client.
     */
    if ((divisor == 0) || (current_msc < target_msc)) {
        /*
         * If target_msc already reached or passed, set it to
         * current_msc to ensure we return a reasonable value back
         * to the caller. This keeps the client from continually
         * sending us MSC targets from the past by forcibly updating
         * their count on this call.
         */
        if (current_msc >= target_msc) {
            target_msc = current_msc;
        }
        vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
        vbl.request.sequence = target_msc;
        vbl.request.signal = (unsigned long)frame_event;

        ret = drmWaitVBlank(vigs_screen->drm->fd, &vbl);

        if (ret != 0) {
            xf86DrvMsg(scrn->scrnIndex, X_WARNING, "drmWaitVBlank failed: %s\n", strerror(errno));
            goto complete;
        }

        frame_event->sequence = vbl.reply.sequence;

        DRI2BlockClient(client, drawable);

        return TRUE;
    }

    /*
     * If we get here, target_msc has already passed or we don't have one,
     * so we queue an event that will satisfy the divisor/remainder equation.
     */
    vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
    vbl.request.sequence = current_msc - (current_msc % divisor) + remainder;

    /*
     * If calculated remainder is larger than requested remainder,
     * it means we've passed the last point where
     * seq % divisor == remainder, so we need to wait for the next time
     * that will happen.
     */
    if ((current_msc % divisor) >= remainder) {
        vbl.request.sequence += divisor;
    }

    vbl.request.signal = (unsigned long)frame_event;

    ret = drmWaitVBlank(vigs_screen->drm->fd, &vbl);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_WARNING, "drmWaitVBlank failed: %s\n", strerror(errno));
        goto complete;
    }

    frame_event->sequence = vbl.reply.sequence;

    DRI2BlockClient(client, drawable);

    return TRUE;

complete:
    if (frame_event) {
        vigs_dri2_client_remove_frame_event(frame_event);
        free(frame_event);
    }

    DRI2WaitMSCComplete(client, drawable, target_msc, 0, 0);

    return TRUE;
}

Bool vigs_dri2_init(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    DRI2InfoRec dri2info;
    int major, minor;

    VIGS_LOG_TRACE("enter");

    if (!vigs_dri2_client_subsystem_init(vigs_screen)) {
        return FALSE;
    }

    AddCallback(&ClientStateCallback, vigs_dri2_client_state_changed, 0);

    memset(&dri2info, 0, sizeof(dri2info));

    if (xf86LoaderCheckSymbol("DRI2Version")) {
        DRI2Version(&major, &minor);
    } else {
        /* Assume version 1.0 */
        major = 1;
        minor = 0;
    }

    dri2info.version = DRI2INFOREC_VERSION;
    dri2info.fd = vigs_screen->drm->fd;
    dri2info.driverName = "vigs";
    dri2info.deviceName = vigs_screen->drm->drm_device_name;

    dri2info.CreateBuffer = &vigs_dri2_create_buffer;
    dri2info.DestroyBuffer = &vigs_dri2_destroy_buffer;
    dri2info.CopyRegion = &vigs_dri2_copy_region;
    dri2info.ScheduleSwap = &vigs_dri2_schedule_swap;
    dri2info.GetMSC = &vigs_dri2_get_msc;
    dri2info.ScheduleWaitMSC = &vigs_dri2_schedule_wait_msc;

    return DRI2ScreenInit(scrn->pScreen, &dri2info);
}

void vigs_dri2_close(struct vigs_screen *vigs_screen)
{
    VIGS_LOG_TRACE("enter");

    DeleteCallback(&ClientStateCallback, vigs_dri2_client_state_changed, 0);

    DRI2CloseScreen(vigs_screen->scrn->pScreen);
}
