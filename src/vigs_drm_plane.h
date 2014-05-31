#ifndef _VIGS_DRM_PLANE_H_
#define _VIGS_DRM_PLANE_H_

#include "vigs_config.h"
#include "vigs_list.h"
#include "xf86.h"
#include "xf86Crtc.h"
#include "xf86drmMode.h"

struct vigs_drm;

struct vigs_drm_plane
{
    /* Link for vigs_drm::planes */
    struct vigs_list list;

    struct vigs_drm *drm;

    int num;

    drmModePlanePtr mode_plane;
};

Bool vigs_drm_plane_init(struct vigs_drm *drm, int num);

void vigs_drm_plane_destroy(struct vigs_drm_plane *plane);

#endif
