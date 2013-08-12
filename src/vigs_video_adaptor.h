#ifndef _VIGS_VIDEO_ADAPTOR_H_
#define _VIGS_VIDEO_ADAPTOR_H_

#include "vigs_config.h"
#include "xf86.h"
#include "xf86xv.h"

struct vigs_video_adaptor
{
    XF86VideoAdaptorRec base;

    void (*destroy)(struct vigs_video_adaptor */*adaptor*/);
};

#endif
