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
