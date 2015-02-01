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
