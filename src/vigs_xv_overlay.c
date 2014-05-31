#include "vigs_xv_overlay.h"
#include "vigs_screen.h"
#include "vigs_drm.h"
#include "vigs_drm_plane.h"
#include "vigs_drm_crtc.h"
#include "vigs_log.h"
#include "vigs.h"

struct vigs_xv_overlay *vigs_xv_overlay_create(struct vigs_drm_plane *plane)
{
    ScrnInfoPtr scrn = plane->drm->screen->scrn;
    struct vigs_xv_overlay *overlay;

    VIGS_LOG_TRACE("plane = %u", plane->mode_plane->plane_id);

    overlay = calloc(1, sizeof(*overlay));

    if (!overlay) {
        xf86DrvMsg(scrn->scrnIndex,
                   X_ERROR, "Unable to allocate overlay for plane %u\n",
                   plane->mode_plane->plane_id);
        return NULL;
    }

    overlay->plane = plane;

    return overlay;
}

void vigs_xv_overlay_destroy(struct vigs_xv_overlay *overlay)
{
    VIGS_LOG_TRACE("plane = %u", overlay->plane->mode_plane->plane_id);

    vigs_xv_overlay_disable(overlay);

    free(overlay);
}

Bool vigs_xv_overlay_enable(struct vigs_xv_overlay *overlay,
                            uint32_t width, uint32_t height)
{
    ScrnInfoPtr scrn = overlay->plane->drm->screen->scrn;
    struct vigs_drm_surface *sfc;
    int ret;

    if (overlay->sfc &&
        (overlay->sfc->width == width) &&
        (overlay->sfc->height == height)) {
        return TRUE;
    }

    ret = vigs_drm_surface_create(overlay->plane->drm->dev,
                                  width,
                                  height,
                                  width * 4,
                                  vigs_drm_surface_bgrx8888,
                                  0,
                                  &sfc);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to create DRM surface: %s\n", strerror(-ret));
        return FALSE;
    }

    if (vigs_xv_overlay_enable_surface(overlay, sfc)) {
        vigs_drm_gem_unref(&sfc->gem);
        return TRUE;
    } else {
        vigs_drm_gem_unref(&sfc->gem);
        return FALSE;
    }
}

Bool vigs_xv_overlay_enable_surface(struct vigs_xv_overlay *overlay,
                                    struct vigs_drm_surface *sfc)
{
    ScrnInfoPtr scrn = overlay->plane->drm->screen->scrn;
    uint32_t fb_id;
    int ret;

    VIGS_LOG_TRACE("plane = %u, sfc_id = %u, width = %u, height = %u",
                   overlay->plane->mode_plane->plane_id,
                   sfc->id, sfc->width, sfc->height);

    ret = drmModeAddFB(overlay->plane->drm->fd,
                       sfc->width,
                       sfc->height,
                       24,
                       32,
                       sfc->stride,
                       sfc->gem.handle,
                       &fb_id);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to add FB: %s\n", strerror(-ret));
        return FALSE;
    }

    vigs_drm_gem_ref(&sfc->gem);

    if (overlay->sfc) {
        /*
         * Just remove FB and GEM, don't call drmModeSetPlane(0),
         * we don't want planes to flicker when continuously resized.
         */
        drmModeRmFB(overlay->plane->drm->fd, overlay->fb_id);
        vigs_drm_gem_unref(&overlay->sfc->gem);
    }

    overlay->sfc = sfc;
    overlay->fb_id = fb_id;

    return TRUE;
}

Bool vigs_xv_overlay_enabled(struct vigs_xv_overlay *overlay)
{
    return (overlay->sfc != NULL);
}

Bool vigs_xv_overlay_update(struct vigs_xv_overlay *overlay,
                            const xRectangle *src_rect,
                            const xRectangle *dst_rect,
                            int zpos)
{
    ScrnInfoPtr scrn = overlay->plane->drm->screen->scrn;
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(scrn);
    int i, ret;
    uint32_t crtc_id = 0;

    if (!overlay->sfc) {
        xf86DrvMsg(scrn->scrnIndex,
                   X_ERROR, "Overlay not enabled, logic error\n");
        return FALSE;
    }

    VIGS_LOG_TRACE("plane = %u, src = {%d, %d, %d, %d}, dst = {%d, %d, %d, %d}",
                   overlay->plane->mode_plane->plane_id,
                   (int)src_rect->x,
                   (int)src_rect->y,
                   (int)src_rect->width,
                   (int)src_rect->height,
                   (int)dst_rect->x,
                   (int)dst_rect->y,
                   (int)dst_rect->width,
                   (int)dst_rect->height);

    ret = vigs_drm_plane_set_zpos(overlay->plane->drm->dev,
                                  overlay->plane->mode_plane->plane_id,
                                  zpos);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to set plane zpos: %s\n", strerror(-ret));
        return FALSE;
    }

    for (i = 0; i < crtc_config->num_crtc; ++i) {
        struct vigs_drm_crtc *crtc = crtc_config->crtc[i]->driver_private;

        crtc_id = crtc->mode_crtc->crtc_id;
    }

    ret = drmModeSetPlane(overlay->plane->drm->fd,
                          overlay->plane->mode_plane->plane_id,
                          crtc_id,
                          overlay->fb_id,
                          0,
                          dst_rect->x, dst_rect->y,
                          dst_rect->width, dst_rect->height,
                          ((uint32_t)src_rect->x) << 16,
                          ((uint32_t)src_rect->y) << 16,
                          ((uint32_t)src_rect->width) << 16,
                          ((uint32_t)src_rect->height) << 16);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to set plane: %s\n", strerror(-ret));
        return FALSE;
    }

    return TRUE;
}

void vigs_xv_overlay_disable(struct vigs_xv_overlay *overlay)
{
    ScrnInfoPtr scrn = overlay->plane->drm->screen->scrn;
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(scrn);
    int i, ret;
    uint32_t crtc_id = 0;

    if (!overlay->sfc) {
        return;
    }

    VIGS_LOG_TRACE("plane = %u", overlay->plane->mode_plane->plane_id);

    for (i = 0; i < crtc_config->num_crtc; ++i) {
        struct vigs_drm_crtc *crtc = crtc_config->crtc[i]->driver_private;

        crtc_id = crtc->mode_crtc->crtc_id;
    }

    ret = drmModeSetPlane(overlay->plane->drm->fd,
                          overlay->plane->mode_plane->plane_id,
                          crtc_id,
                          0,
                          0,
                          0, 0,
                          0, 0,
                          0, 0,
                          0, 0);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to disable plane: %s\n", strerror(-ret));
    }

    drmModeRmFB(overlay->plane->drm->fd, overlay->fb_id);
    overlay->fb_id = 0;

    vigs_drm_gem_unref(&overlay->sfc->gem);
    overlay->sfc = NULL;
}
