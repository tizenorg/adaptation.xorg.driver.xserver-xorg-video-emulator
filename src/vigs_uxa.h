#ifndef _VIGS_UXA_H_
#define _VIGS_UXA_H_

#include "vigs_config.h"
#include "xf86.h"

struct vigs_screen;
struct vigs_pixmap;

typedef enum
{
    vigs_uxa_access_read = 1,
    vigs_uxa_access_write = 2,
    vigs_uxa_access_readwrite = vigs_uxa_access_read | vigs_uxa_access_write
} vigs_uxa_access;

Bool vigs_uxa_init(struct vigs_screen *vigs_screen);

Bool vigs_uxa_create_screen_resources(struct vigs_screen *vigs_screen);

void vigs_uxa_close(struct vigs_screen *vigs_screen);

void vigs_uxa_flush(struct vigs_screen *vigs_screen);

/*
 * Start/end access to pixmap's data. These are relatively heavy
 * operations, since they require:
 * + Mapping pixmap into system memory, possibly
 *   downloading contents from GPU to VRAM.
 * + In case if 'access' contains 'write' it'll also mark pixmap as VRAM dirty,
 *   so it'll be uploaded from VRAM to GPU on next command buffer flush.
 * @{
 */

Bool vigs_uxa_raw_access(struct vigs_pixmap *vigs_pixmap,
                         int x, int y,
                         int w, int h,
                         vigs_uxa_access access);

void vigs_uxa_end_raw_access(struct vigs_pixmap *vigs_pixmap);

/*
 * @}
 */

#endif
