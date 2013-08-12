#ifndef _VIGS_DRM_H_
#define _VIGS_DRM_H_

#include "vigs_config.h"
#include "xf86.h"
#include "xf86drmMode.h"
#include "xf86drm.h"

struct vigs_drm_device;
struct vigs_drm_surface;
struct vigs_screen;
struct vigs_dri2_frame_event;

struct vigs_drm
{
    struct vigs_screen *screen;

    int fd;

    char* drm_device_name;

    struct vigs_drm_device *dev;

    drmModeResPtr mode_res;

    drmEventContext event_context;

    uint32_t fb_id;
};

Bool vigs_drm_pre_init(struct vigs_screen *vigs_screen,
                       const char *bus_id);

Bool vigs_drm_init(struct vigs_screen *vigs_screen);

void vigs_drm_close(struct vigs_screen *vigs_screen);

void vigs_drm_free(struct vigs_screen *vigs_screen);

Bool vigs_drm_set_master(struct vigs_drm *drm);

void vigs_drm_drop_master(struct vigs_drm *drm);

Bool vigs_drm_pageflip(struct vigs_drm *drm,
                       struct vigs_dri2_frame_event *frame_event,
                       struct vigs_drm_surface *sfc);

#endif
