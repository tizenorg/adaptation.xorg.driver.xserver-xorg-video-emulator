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
                            int zpos);

void vigs_xv_overlay_disable(struct vigs_xv_overlay *overlay);

#endif
