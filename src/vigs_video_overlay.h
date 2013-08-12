#ifndef _VIGS_VIDEO_OVERLAY_H_
#define _VIGS_VIDEO_OVERLAY_H_

#include "vigs_config.h"
#include "xf86.h"

struct vigs_screen;

struct vigs_video_overlay
{
    struct vigs_screen *screen;

    char *device_name;

    int fd;

    xRectangle viewport;

    /*
     * mmaped area of overlay's viewport.
     * @{
     */

    unsigned char *mmap_ptr;
    int mmap_size;

    /*
     * @}
     */

    int is_stream_on;
};

struct vigs_video_overlay
    *vigs_video_overlay_create(struct vigs_screen *screen,
                               const char *device_name);

void vigs_video_overlay_destroy(struct vigs_video_overlay *overlay);

Bool vigs_video_overlay_set_viewport(struct vigs_video_overlay *overlay,
                                     xRectangle *viewport);

xRectangle *vigs_video_overlay_viewport(struct vigs_video_overlay *overlay);

unsigned char *vigs_video_overlay_ptr(struct vigs_video_overlay *overlay);

Bool vigs_video_overlay_stream_on(struct vigs_video_overlay *overlay);

void vigs_video_overlay_stream_off(struct vigs_video_overlay *overlay);

#endif
