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

#ifndef _VIGS_SCREEN_H_
#define _VIGS_SCREEN_H_

#include "vigs_config.h"
#include "vigs_list.h"
#include "vigs_options.h"
#include "xf86.h"
#include "uxa/uxa.h"

#include "compat-api.h"

struct vigs_drm;
struct vigs_comm;
struct vigs_xv;
struct vigs_drm_surface;

struct vigs_screen
{
    ScrnInfoPtr scrn;

    EntityInfoPtr ent;

    OptionInfoRec options[vigs_option_count + 1];

    /*
     * Set through X.Org options.
     * @{
     */
    uint32_t max_execbuffer_size;
    int no_accel;
    /*
     * @}
     */

    /*
     * Set through XRANDR.
     * @{
     */
    int vsync;
    int pageflip;
    /*
     * @}
     */

    struct vigs_drm *drm;

    struct vigs_comm *comm;

    struct vigs_drm_surface *front_sfc;

    uxa_driver_t *uxa_driver;

    struct vigs_xv *xv;

    CloseScreenProcPtr close_screen_fn;
    CreateScreenResourcesProcPtr create_screen_resources_fn;
    ScreenBlockHandlerProcPtr block_handler_fn;

    int pre_initialized;

    int initialized;

    /*
     * A list of pixmaps that have dirty VRAM.
     */
    struct vigs_list dirty_vram_pixmaps;
};

Bool vigs_screen_pre_init(ScrnInfoPtr scrn, int flags);

Bool vigs_screen_init(SCREEN_INIT_ARGS_DECL);

Bool vigs_screen_switch_mode(SWITCH_MODE_ARGS_DECL);

void vigs_screen_adjust_frame(ADJUST_FRAME_ARGS_DECL);

ModeStatus vigs_screen_valid_mode(SCRN_ARG_TYPE arg, DisplayModePtr mode, Bool verbose, int flags);

Bool vigs_screen_enter_vt(VT_FUNC_ARGS_DECL);

void vigs_screen_leave_vt(VT_FUNC_ARGS_DECL);

void vigs_screen_free(FREE_SCREEN_ARGS_DECL);

#endif
