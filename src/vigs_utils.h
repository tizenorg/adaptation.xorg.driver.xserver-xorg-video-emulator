#ifndef _VIGS_UTILS_H_
#define _VIGS_UTILS_H_

#include "vigs_config.h"
#include "xf86drm.h"
#include "xf86drmMode.h"
#include "xf86Crtc.h"
#include <pixman.h>

#define vigs_offsetof(type, member) ((size_t)&((type*)0)->member)

#define vigs_containerof(ptr, type, member) ((type*)((char*)(ptr) - vigs_offsetof(type, member)))

#define vigs_max(a,b) (((a) > (b)) ? (a) : (b))
#define vigs_min(a,b) (((a) < (b)) ? (a) : (b))

void vigs_drm_mode_to_mode(ScrnInfoPtr scrn,
                           drmModeModeInfo *drm_mode,
                           DisplayModePtr mode);

void vigs_mode_to_drm_mode(ScrnInfoPtr scrn,
                           DisplayModePtr mode,
                           drmModeModeInfo *drm_mode);

PropertyPtr vigs_get_window_property(WindowPtr window,
                                     const char *property_name);

PixmapPtr vigs_get_drawable_pixmap(DrawablePtr drawable);

void vigs_pixman_convert_image(pixman_op_t op,
                               unsigned char *srcbuf,
                               unsigned char *dstbuf,
                               pixman_format_code_t src_format,
                               pixman_format_code_t dst_format,
                               xRectangle *img_rect,
                               xRectangle *pxm_rect,
                               xRectangle *src_rect,
                               xRectangle *dst_rect,
                               RegionPtr clip_region,
                               int rotation,
                               int is_hflip,
                               int is_vflip);

uint64_t vigs_gettime_us();

void *vigs_copy_image(int width, int height,
                      char *s, int s_size_w, int s_size_h,
                      int *s_pitches, int *s_offsets, int *s_lengths,
                      char *d, int d_size_w, int d_size_h,
                      int *d_pitches, int *d_offsets, int *d_lengths,
                      int channel, int h_sampling, int v_sampling);

#endif
