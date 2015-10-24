#ifndef _VIGS_PRESENT_H_
#define _VIGS_PRESENT_H_

#include "vigs_config.h"
#include "xf86.h"
#include "present.h"

struct vigs_screen;

Bool vigs_present_init(struct vigs_screen *vigs_screen);

void vigs_present_close(struct vigs_screen *vigs_screen);

#endif /* _VIGS_PRESENT_H_ */
