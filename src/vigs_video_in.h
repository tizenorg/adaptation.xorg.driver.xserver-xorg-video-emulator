#ifndef _VIGS_VIDEO_IN_H_
#define _VIGS_VIDEO_IN_H_

#include "vigs_config.h"

struct vigs_video;
struct vigs_video_adaptor;

struct vigs_video_adaptor *vigs_video_in_create(struct vigs_video *xv);

#endif
