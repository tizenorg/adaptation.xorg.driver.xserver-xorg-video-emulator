#ifndef _VIGS_DRM_OUTPUT_H_
#define _VIGS_DRM_OUTPUT_H_

#include "vigs_config.h"
#include "xf86.h"
#include "xf86Crtc.h"
#include "xf86drmMode.h"

struct vigs_drm;

struct vigs_drm_output
{
    struct vigs_drm *drm;

    int num;

    drmModeConnectorPtr mode_connector;
    drmModeEncoderPtr mode_encoder;

    int dpms_mode;
};

Bool vigs_drm_output_init(struct vigs_drm *drm, int num);

void vigs_drm_output_dpms(xf86OutputPtr output, int mode);

#endif
