#ifndef _VIGS_XV_H_
#define _VIGS_XV_H_

#include "vigs_config.h"
#include "xf86.h"
#include "xf86xv.h"

#define VIGS_NUM_XV_ADAPTORS 2

struct vigs_screen;
struct vigs_xv_overlay;

struct vigs_xv
{
    /* Screen on which we're on. */
    struct vigs_screen *screen;

    /* Xv adaptors for this screen. */
    XF86VideoAdaptorPtr adaptors[VIGS_NUM_XV_ADAPTORS];

    /* Video overlays for display device. */
    struct vigs_xv_overlay **overlays;
    int num_overlays;
};

Bool vigs_xv_init(struct vigs_screen *vigs_screen);

void vigs_xv_close(struct vigs_screen *vigs_screen);

#endif
