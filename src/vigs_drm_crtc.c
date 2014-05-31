#include "vigs_drm_crtc.h"
#include "vigs_drm.h"
#include "vigs_drm_output.h"
#include "vigs_utils.h"
#include "vigs_screen.h"
#include "vigs_log.h"
#include "vigs.h"
#include "xf86drm.h"
#include <X11/extensions/dpmsconst.h>

static void vigs_drm_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
    struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;

    VIGS_LOG_TRACE("%d: mode = %d", vigs_crtc->num, mode);
}

static Bool vigs_drm_crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
                                         Rotation rotation, int x, int y)
{
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
    struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;
    struct vigs_drm *vigs_drm = vigs_crtc->drm;
    ScrnInfoPtr scrn = vigs_drm->screen->scrn;
    uint32_t *output_ids;
    int output_count = 0;
    int i;
    int ret;
    drmModeModeInfo drm_mode;

    VIGS_LOG_TRACE("%d: x = %d, y = %d", vigs_crtc->num, x, y);

    if (vigs_drm->fb_id == 0) {
        ret = drmModeAddFB(vigs_drm->fd,
                           vigs_drm->screen->front_sfc->width,
                           vigs_drm->screen->front_sfc->height,
                           scrn->depth,
                           scrn->bitsPerPixel,
                           vigs_drm->screen->front_sfc->stride,
                           vigs_drm->screen->front_sfc->gem.handle,
                           &vigs_drm->fb_id);

        if (ret < 0) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to add FB: %s\n", strerror(-ret));
            return FALSE;
        }
    }

    if (!xf86CrtcRotate(crtc)) {
        return FALSE;
    }

    output_ids = calloc(sizeof(uint32_t), crtc_config->num_output);

    if (!output_ids) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate memory for outputs\n");
        return FALSE;
    }

    for (i = 0; i < crtc_config->num_output; ++i) {
        xf86OutputPtr output = crtc_config->output[i];
        struct vigs_drm_output *vigs_output = output->driver_private;

        if (output->crtc != crtc) {
            continue;
        }

        output_ids[output_count] = vigs_output->mode_connector->connector_id;
        ++output_count;
    }

    vigs_mode_to_drm_mode(scrn, mode, &drm_mode);

    ret = drmModeSetCrtc(vigs_drm->fd,
                         vigs_crtc->mode_crtc->crtc_id,
                         vigs_drm->fb_id,
                         x,
                         y,
                         output_ids,
                         output_count,
                         &drm_mode);
    free(output_ids);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to set mode: %s\n", strerror(-ret));
        return FALSE;
    }

    /*
     * Workarounds for X server improper behavior.
     */

    for (i = 0; i < crtc_config->num_output; ++i) {
        xf86OutputPtr output = crtc_config->output[i];

        if (output->crtc != crtc) {
            continue;
        }

        vigs_drm_output_dpms(output, DPMSModeOn);
    }

    crtc->funcs->gamma_set(crtc,
                           crtc->gamma_red,
                           crtc->gamma_green,
                           crtc->gamma_blue,
                           crtc->gamma_size);

    return TRUE;
}

static void vigs_drm_crtc_gamma_set(xf86CrtcPtr crtc,
                                    CARD16 *red,
                                    CARD16 *green,
                                    CARD16 *blue,
                                    int size)
{
    struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;

    VIGS_LOG_TRACE("%d: size = %d", vigs_crtc->num, size);
}

static void vigs_drm_crtc_destroy(xf86CrtcPtr crtc)
{
    struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;

    VIGS_LOG_TRACE("%d", vigs_crtc->num);

    drmModeFreeCrtc(vigs_crtc->mode_crtc);

    free(vigs_crtc);
}

static void vigs_drm_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{
}

static void vigs_drm_crtc_set_cursor_position(xf86CrtcPtr crtc, int x, int y)
{
}

static void vigs_drm_crtc_show_cursor(xf86CrtcPtr crtc)
{
    VIGS_LOG_TRACE("enter");
}

static void vigs_drm_crtc_hide_cursor(xf86CrtcPtr crtc)
{
    VIGS_LOG_TRACE("enter");
}

static void vigs_drm_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{
}

static const xf86CrtcFuncsRec vigs_drm_crtc_funcs =
{
    .dpms = vigs_drm_crtc_dpms,
    .set_mode_major = vigs_drm_crtc_set_mode_major,
    .gamma_set = vigs_drm_crtc_gamma_set,
    .destroy = vigs_drm_crtc_destroy,
    /*
     * Cursor stuff.
     * @{
     */
    .set_cursor_colors = vigs_drm_crtc_set_cursor_colors,
    .set_cursor_position = vigs_drm_crtc_set_cursor_position,
    .show_cursor = vigs_drm_crtc_show_cursor,
    .hide_cursor = vigs_drm_crtc_hide_cursor,
    .load_cursor_argb = vigs_drm_crtc_load_cursor_argb,
    /*
     * @}
     */
};

Bool vigs_drm_crtc_init(struct vigs_drm *drm, int num)
{
    xf86CrtcPtr crtc;
    struct vigs_drm_crtc *vigs_crtc;

    VIGS_LOG_TRACE("%d", num);

    crtc = xf86CrtcCreate(drm->screen->scrn, &vigs_drm_crtc_funcs);

    if (!crtc) {
        return FALSE;
    }

    vigs_crtc = xnfcalloc(sizeof(*vigs_crtc), 1);

    crtc->driver_private = vigs_crtc;

    vigs_crtc->drm = drm;
    vigs_crtc->num = num;
    vigs_crtc->mode_crtc = drmModeGetCrtc(drm->fd,
                                          drm->mode_res->crtcs[num]);

    if (!vigs_crtc->mode_crtc) {
        return FALSE;
    }

    return TRUE;
}

Bool vigs_drm_crtc_is_on(xf86CrtcPtr crtc)
{
    struct vigs_drm_crtc *vigs_crtc = crtc->driver_private;
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
    drmModeCrtcPtr mode_crtc;
    Bool ret;
    int i;

    if (!crtc->enabled) {
        return FALSE;
    }

    ret = FALSE;

    for (i = 0; i < crtc_config->num_output; ++i) {
        xf86OutputPtr output = crtc_config->output[i];
        struct vigs_drm_output *vigs_output = output->driver_private;

        if ((output->crtc == crtc) &&
            (vigs_output->dpms_mode == DPMSModeOn)) {
            ret = TRUE;
            break;
        }
    }

    if (!ret) {
        return FALSE;
    }

    mode_crtc = drmModeGetCrtc(vigs_crtc->drm->fd,
                               vigs_crtc->mode_crtc->crtc_id);

    if (mode_crtc == NULL) {
        return FALSE;
    }

    ret = (mode_crtc->mode_valid &&
           (vigs_crtc->drm->fb_id == mode_crtc->buffer_id));

    drmModeFreeCrtc(mode_crtc);

    return ret;
}
