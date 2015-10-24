/*
 * X.Org X server driver for VIGS
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact :
 * Vasiliy Ulyanov<v.ulyanov@samsung.com>
 * Jinhyung Jo <jinhyung.jo@samsung.com>
 * Sangho Park <sangho1206.park@samsung.com>
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

#ifndef _VIGS_CURSOR_H_
#define _VIGS_CURSOR_H_

#include "vigs_config.h"
#include "xf86.h"

struct vigs_screen;
struct vigs_xv_overlay;

struct vigs_cursor {
    /* Screen on which the cursor is on */
    struct vigs_screen *screen;

    /* Cursor overlay */
    struct vigs_xv_overlay *overlay;

    /* DRM surface for the cursor */
    struct vigs_drm_surface *sfc;

    /* Current cursor position */
    int x;
    int y;
};

Bool vigs_cursor_init(struct vigs_screen *vigs_screen);
void vigs_cursor_close(struct vigs_screen *vigs_screen);

void vigs_cursor_load_argb(struct vigs_cursor *cursor, void *image);
void vigs_cursor_set_position(struct vigs_cursor *cursor, int x, int y,
                              int update);
void vigs_cursor_show(struct vigs_cursor *cursor);
void vigs_cursor_hide(struct vigs_cursor *cursor);

#endif
