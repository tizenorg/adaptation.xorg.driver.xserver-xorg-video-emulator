#ifndef _VIGS_DRI3_H_
#define _VIGS_DRI3_H_

#include "vigs_config.h"
#include "xf86.h"
#include "dri3.h"
#include "misyncshm.h"

struct vigs_screen;

Bool vigs_dri3_init(struct vigs_screen *vigs_screen);

void vigs_dri3_close(struct vigs_screen *vigs_screen);

#endif /* _VIGS_DRI3_H_ */
