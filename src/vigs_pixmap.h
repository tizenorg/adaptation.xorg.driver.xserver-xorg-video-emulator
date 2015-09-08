/*
 * X.Org X server driver for VIGS
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact :
 * Stanislav Vorobiov <s.vorobiov@samsung.com>
 * Jinhyung Jo <jinhyung.jo@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#ifndef _VIGS_PIXMAP_H_
#define _VIGS_PIXMAP_H_

#include "vigs_config.h"
#include "xf86.h"
#include "vigs_protocol.h"
#include "vigs_list.h"

struct vigs_screen;
struct vigs_drm_surface;

struct vigs_pixmap
{
    /* Link for vigs_screen::vram_dirty_pixmaps */
    struct vigs_list list;

    /* Screen on which we're on. */
    struct vigs_screen *screen;

    /* Pixmap we're managing. */
    PixmapPtr pixmap;

    /* DRM surface for this pixmap. Allocated on first access. */
    struct vigs_drm_surface *sfc;

    /*
     * VRAM dirty region. Allocated on first access.
     * Need to update GPU when it's not empty.
     */
    RegionPtr vram_dirty_region;

    /* GPU is dirty, need to update VRAM. */
    int is_gpu_dirty;
};

#if HAS_DEVPRIVATEKEYREC
extern DevPrivateKeyRec vigs_pixmap_index;
#else
extern int vigs_pixmap_index;
#endif

static inline struct vigs_pixmap *pixmap_to_vigs_pixmap(PixmapPtr pixmap)
{
#if HAS_DIXREGISTERPRIVATEKEY
    return dixGetPrivate(&pixmap->devPrivates, &vigs_pixmap_index);
#else
    return dixLookupPrivate(&pixmap->devPrivates, &vigs_pixmap_index);
#endif
}

/*
 * Use with extreme caution, generally this is for DRI2 only.
 */
static inline void vigs_pixmap_set_private(PixmapPtr pixmap,
                                           struct vigs_pixmap *vigs_pixmap)
{
    dixSetPrivate(&pixmap->devPrivates, &vigs_pixmap_index, vigs_pixmap);
}

static inline uint32_t vigs_pixmap_width(PixmapPtr pixmap)
{
    return pixmap->drawable.width;
}

static inline uint32_t vigs_pixmap_height(PixmapPtr pixmap)
{
    return pixmap->drawable.height;
}

static inline uint32_t vigs_pixmap_stride(PixmapPtr pixmap)
{
    return pixmap->devKind;
}

static inline uint32_t vigs_pixmap_depth(PixmapPtr pixmap)
{
    return pixmap->drawable.depth;
}

/*
 * Bytes-per-pixel.
 */
static inline uint32_t vigs_pixmap_bpp(PixmapPtr pixmap)
{
    return (pixmap->drawable.bitsPerPixel + 7) / 8;
}

Bool vigs_pixmap_subsystem_init(struct vigs_screen *vigs_screen);

PixmapPtr vigs_pixmap_create(struct vigs_screen *vigs_screen,
                             uint32_t width,
                             uint32_t height,
                             uint32_t depth,
                             unsigned int usage);

/*
 * If vigs_pixmap is already created then it's recreated. Also, this
 * will alter pixmap header according to 'sfc' format.
 */
Bool vigs_pixmap_create_from_surface(PixmapPtr pixmap,
                                     struct vigs_drm_surface *sfc);

void vigs_pixmap_destroy(PixmapPtr pixmap);

void vigs_pixmap_exchange(struct vigs_screen *vigs_screen,
                          PixmapPtr dest,
                          PixmapPtr src);

void vigs_pixmap_set_vram_dirty(struct vigs_pixmap *vigs_pixmap,
                                int x, int y, int w, int h);

void vigs_pixmap_set_vram_not_dirty(struct vigs_pixmap *vigs_pixmap);

int vigs_pixmap_is_vram_dirty(struct vigs_pixmap *vigs_pixmap);

void vigs_pixmap_set_gpu_dirty(struct vigs_pixmap *vigs_pixmap, int is_dirty);

int vigs_pixmap_is_gpu_dirty(struct vigs_pixmap *vigs_pixmap);

/*
 * Create a surface for vigs_pixmap if it doesn't have one already.
 * If it does then this is a no-op.
 */
Bool vigs_pixmap_create_sfc(PixmapPtr pixmap);

Bool vigs_pixmap_start_access(PixmapPtr pixmap, int is_read, int is_write);

void vigs_pixmap_end_access(PixmapPtr pixmap);

Bool vigs_pixmap_get_name(PixmapPtr pixmap);

#endif
