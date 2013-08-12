#ifndef _VIGS_VIDEO_H_
#define _VIGS_VIDEO_H_

#include "vigs_config.h"
#include "xf86.h"
#include "xf86xv.h"

#define VIGS_NUM_VIDEO_ADAPTORS 2
#define VIGS_NUM_VIDEO_OVERLAYS 2

struct vigs_screen;
struct vigs_video_adaptor;
struct vigs_video_overlay;

struct vigs_video
{
    /* Screen on which we're on. */
    struct vigs_screen *screen;

    /* Xv adaptors for this screen. */
    XF86VideoAdaptorPtr adaptors[VIGS_NUM_VIDEO_ADAPTORS];

    /* Video overlays for display device. */
    struct vigs_video_overlay *overlays[VIGS_NUM_VIDEO_OVERLAYS];
};

Bool vigs_video_init(struct vigs_screen *vigs_screen);

void vigs_video_close(struct vigs_screen *vigs_screen);

/*
 * Returns -1 on error or if no free overlays are available.
 */
int vigs_video_open_overlay(struct vigs_video *xv);

void vigs_video_close_overlay(struct vigs_video *xv, int overlay_index);

#endif
