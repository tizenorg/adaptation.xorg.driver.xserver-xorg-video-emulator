#ifndef _VIGS_VIDEO_OUT_H_
#define _VIGS_VIDEO_OUT_H_

#include "vigs_config.h"

struct vigs_video;
struct vigs_video_adaptor;

struct vigs_video_adaptor *vigs_video_out_create(struct vigs_video *xv);

#endif
