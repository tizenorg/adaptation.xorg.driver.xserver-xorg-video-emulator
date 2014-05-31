#ifndef _VIGS_DRM_CRTC_H_
#define _VIGS_DRM_CRTC_H_

#include "vigs_config.h"
#include "xf86.h"
#include "xf86Crtc.h"
#include "xf86drmMode.h"

struct vigs_drm;

struct vigs_drm_crtc
{
    struct vigs_drm *drm;

    int num;

    drmModeCrtcPtr mode_crtc;
};

Bool vigs_drm_crtc_init(struct vigs_drm *drm, int num);

Bool vigs_drm_crtc_is_on(xf86CrtcPtr crtc);

#endif
