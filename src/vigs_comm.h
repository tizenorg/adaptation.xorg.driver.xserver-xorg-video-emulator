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

#ifndef _VIGS_COMM_H_
#define _VIGS_COMM_H_

#include "vigs_config.h"
#include "xf86.h"
#include "vigs_protocol.h"

struct vigs_screen;
struct vigs_drm_execbuffer;

struct vigs_comm
{
    /* Screen we're on. */
    struct vigs_screen *screen;

    /* Execbuffer will not grow beyond this. */
    uint32_t max_size;

    /* Execbuffer that holds command data. */
    struct vigs_drm_execbuffer *execbuffer;

    /* Current command. */
    vigsp_cmd cmd;

    /* Pointer to start of current command. */
    void *cmd_ptr;

    /* Command size so far. */
    uint32_t cmd_size;

    /* Allocation failed, just reset the whole thing when command is done. */
    int alloc_failed;
};

Bool vigs_comm_create(struct vigs_screen *vigs_screen,
                      uint32_t max_size,
                      struct vigs_comm **comm);

void vigs_comm_destroy(struct vigs_comm *comm);

Bool vigs_comm_flush(struct vigs_comm *comm);

void vigs_comm_update_vram(struct vigs_comm *comm,
                           vigsp_surface_id sfc_id);

void vigs_comm_update_gpu(struct vigs_comm *comm,
                          vigsp_surface_id sfc_id,
                          RegionPtr region);

void vigs_comm_copy_prepare(struct vigs_comm *comm,
                            vigsp_surface_id src_id,
                            vigsp_surface_id dst_id);

void vigs_comm_copy(struct vigs_comm *comm,
                    int src_x1, int src_y1,
                    int dst_x1, int dst_y1,
                    int w, int h);

void vigs_comm_copy_done(struct vigs_comm *comm);

void vigs_comm_solid_fill_prepare(struct vigs_comm *comm,
                                  vigsp_surface_id sfc_id,
                                  vigsp_color color);

void vigs_comm_solid_fill(struct vigs_comm *comm,
                          int x1, int y1,
                          int x2, int y2);

void vigs_comm_solid_fill_done(struct vigs_comm *comm);

#endif
