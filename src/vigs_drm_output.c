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

#include "vigs_drm_output.h"
#include "vigs_drm.h"
#include "vigs_log.h"
#include "vigs_utils.h"
#include "vigs_screen.h"
#include "xf86drm.h"
#include <X11/Xatom.h>
#include <X11/extensions/dpmsconst.h>

static const char *vigs_drm_connector_names[] =
{
    [DRM_MODE_CONNECTOR_Unknown] = "Unknown",
    [DRM_MODE_CONNECTOR_VGA] = "VGA",
    [DRM_MODE_CONNECTOR_DVII] = "DVII",
    [DRM_MODE_CONNECTOR_DVID] = "DVID",
    [DRM_MODE_CONNECTOR_DVIA] = "DVIA",
    [DRM_MODE_CONNECTOR_Composite] = "Composite",
    [DRM_MODE_CONNECTOR_SVIDEO] = "SVIDEO",
    [DRM_MODE_CONNECTOR_LVDS] = "LVDS",
    [DRM_MODE_CONNECTOR_Component] = "Component",
    [DRM_MODE_CONNECTOR_9PinDIN] = "9PinDIN",
    [DRM_MODE_CONNECTOR_DisplayPort] = "DisplayPort",
    [DRM_MODE_CONNECTOR_HDMIA] = "HDMIA",
    [DRM_MODE_CONNECTOR_HDMIB] = "HDMIB",
    [DRM_MODE_CONNECTOR_TV] = "TV",
    [DRM_MODE_CONNECTOR_eDP] = "eDP",
    [DRM_MODE_CONNECTOR_VIRTUAL] = "Virtual"
};

#define VIGS_PROPERTY_VSYNC "vsync"
static Atom g_vsync_atom = None;

#define VIGS_PROPERTY_PAGEFLIP "pageflip"
static Atom g_pageflip_atom = None;

static void vigs_drm_output_create_bool_property(xf86OutputPtr output,
                                                 Atom *atom,
                                                 const char *name,
                                                 Bool def,
                                                 Bool immutable)
{
    INT32 atom_values[] = { FALSE, TRUE };
    int ret;

    *atom = MakeAtom(name, strlen(name), TRUE);

    ret = RRConfigureOutputProperty(output->randr_output,
                                    *atom,
                                    FALSE,
                                    FALSE,
                                    immutable,
                                    sizeof(atom_values)/sizeof(atom_values[0]),
                                    atom_values);
    if (ret != 0) {
        xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "RRConfigureOutputProperty error: %d\n", ret);
        return;
    }

    ret = RRChangeOutputProperty(output->randr_output,
                                 *atom,
                                 XA_INTEGER,
                                 32,
                                 PropModeReplace, 1, &def, FALSE,
                                 TRUE);
    if (ret != 0) {
        xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "RRChangeOutputProperty error: %d\n", ret);
        return;
    }
}

static void vigs_drm_output_create_resources(xf86OutputPtr output)
{
    struct vigs_drm_output *vigs_output = output->driver_private;

    VIGS_LOG_TRACE("%d: enter", vigs_output->num);

    /*
     * VSYNC is on by default.
     */
    vigs_drm_output_create_bool_property(output,
                                         &g_vsync_atom,
                                         VIGS_PROPERTY_VSYNC,
                                         TRUE,
                                         FALSE);

    /*
     * pageflip is on by default.
     */
    vigs_drm_output_create_bool_property(output,
                                         &g_pageflip_atom,
                                         VIGS_PROPERTY_PAGEFLIP,
                                         TRUE,
                                         FALSE);
}

static xf86OutputStatus vigs_drm_output_detect(xf86OutputPtr output)
{
    struct vigs_drm_output *vigs_output = output->driver_private;

    VIGS_LOG_TRACE("%d: enter", vigs_output->num);

    return XF86OutputStatusConnected;
}

static Bool vigs_drm_output_mode_valid(xf86OutputPtr output, DisplayModePtr mode)
{
    struct vigs_drm_output *vigs_output = output->driver_private;

    VIGS_LOG_TRACE("%d: enter", vigs_output->num);

    return MODE_OK;
}

static DisplayModePtr vigs_drm_output_get_modes(xf86OutputPtr output)
{
    struct vigs_drm_output *vigs_output = output->driver_private;
    DisplayModePtr modes = NULL;
    DisplayModePtr mode;
    int i;

    VIGS_LOG_TRACE("%d: %d modes",
                   vigs_output->num,
                   vigs_output->mode_connector->count_modes);

    for (i = 0; i < vigs_output->mode_connector->count_modes; i++) {
        mode = xnfalloc(sizeof(*mode));

        vigs_drm_mode_to_mode(output->scrn,
                              &vigs_output->mode_connector->modes[i],
                              mode);

        modes = xf86ModesAdd(modes, mode);
    }

    return modes;
}

static Bool vigs_drm_output_set_property(xf86OutputPtr output,
                                         Atom property,
                                         RRPropertyValuePtr value)
{
    struct vigs_drm_output *vigs_output = output->driver_private;

    if (property == g_vsync_atom) {
        INT32 tmp;

        if ((value->type != XA_INTEGER) ||
            (value->format != 32) ||
            (value->size != 1)) {
            VIGS_LOG_ERROR("%d: vsync = ?", vigs_output->num);
            return FALSE;
        }

        tmp = *(INT32*)value->data;

        VIGS_LOG_TRACE("%d: vsync = %d", vigs_output->num, tmp);

        vigs_output->drm->screen->vsync = tmp;
    } else if (property == g_pageflip_atom) {
        INT32 tmp;

        if ((value->type != XA_INTEGER) ||
            (value->format != 32) ||
            (value->size != 1)) {
            VIGS_LOG_ERROR("%d: pageflip = ?", vigs_output->num);
            return FALSE;
        }

        tmp = *(INT32*)value->data;

        VIGS_LOG_TRACE("%d: pageflip = %d", vigs_output->num, tmp);

        vigs_output->drm->screen->pageflip = tmp;
    } else {
        VIGS_LOG_TRACE("%d: enter", vigs_output->num);
    }

    return TRUE;
}

static Bool vigs_drm_output_get_property(xf86OutputPtr output, Atom property)
{
    struct vigs_drm_output *vigs_output = output->driver_private;
    int ret;

    if (property == g_vsync_atom) {
        Bool value = vigs_output->drm->screen->vsync;

        VIGS_LOG_TRACE("%d: vsync = %d", vigs_output->num, value);

        ret = RRChangeOutputProperty(output->randr_output,
                                     property,
                                     XA_INTEGER,
                                     32,
                                     PropModeReplace,
                                     1,
                                     &value,
                                     FALSE,
                                     TRUE);
        if (ret != 0) {
            xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "RRChangeOutputProperty error: %d\n", ret);
        } else {
            return TRUE;
        }
    } else if (property == g_pageflip_atom) {
        Bool value = vigs_output->drm->screen->pageflip;

        VIGS_LOG_TRACE("%d: pageflip = %d", vigs_output->num, value);

        ret = RRChangeOutputProperty(output->randr_output,
                                     property,
                                     XA_INTEGER,
                                     32,
                                     PropModeReplace,
                                     1,
                                     &value,
                                     FALSE,
                                     TRUE);
        if (ret != 0) {
            xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "RRChangeOutputProperty error: %d\n", ret);
        } else {
            return TRUE;
        }
    } else {
        VIGS_LOG_TRACE("%d: enter", vigs_output->num);
    }

    return FALSE;
}

static void vigs_drm_output_destroy(xf86OutputPtr output)
{
    struct vigs_drm_output *vigs_output = output->driver_private;

    VIGS_LOG_TRACE("%d", vigs_output->num);

    drmModeFreeEncoder(vigs_output->mode_encoder);
    drmModeFreeConnector(vigs_output->mode_connector);

    free(vigs_output);
}

static const xf86OutputFuncsRec vigs_drm_output_funcs =
{
    .create_resources = vigs_drm_output_create_resources,
    .dpms = vigs_drm_output_dpms,
    .detect = vigs_drm_output_detect,
    .mode_valid = vigs_drm_output_mode_valid,
    .get_modes = vigs_drm_output_get_modes,
    .set_property = vigs_drm_output_set_property,
    .get_property = vigs_drm_output_get_property,
    .destroy = vigs_drm_output_destroy
};

Bool vigs_drm_output_init(struct vigs_drm *drm, int num)
{
    drmModeConnectorPtr connector;
    drmModeEncoderPtr encoder;
    char name[32];
    xf86OutputPtr output;
    struct vigs_drm_output *vigs_output;

    VIGS_LOG_TRACE("%d", num);

    connector = drmModeGetConnector(drm->fd,
                                    drm->mode_res->connectors[num]);
    if (!connector) {
        return FALSE;
    }

    encoder = drmModeGetEncoder(drm->fd, connector->encoders[0]);

    if (!encoder) {
        drmModeFreeConnector(connector);
        return FALSE;
    }

    if (connector->connector_type >=
        (sizeof(vigs_drm_connector_names)/sizeof(vigs_drm_connector_names[0]))) {
        snprintf(name, 32, "Unknown%d-%d", connector->connector_type,
                                           connector->connector_type_id);
    } else {
        snprintf(name, 32, "%s%d", vigs_drm_connector_names[connector->connector_type],
                                   connector->connector_type_id);
    }

    output = xf86OutputCreate(drm->screen->scrn, &vigs_drm_output_funcs, name);

    if (!output) {
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        return FALSE;
    }

    vigs_output = xnfcalloc(sizeof(*vigs_output), 1);

    output->driver_private = vigs_output;
    output->mm_width = connector->mmWidth;
    output->mm_height = connector->mmHeight;
    if (output->conf_monitor) {
        output->conf_monitor->mon_width = output->mm_width;
        output->conf_monitor->mon_height = output->mm_height;
    }
    output->possible_crtcs = encoder->possible_crtcs;
    output->possible_clones = encoder->possible_clones;
    output->interlaceAllowed = FALSE;
    output->doubleScanAllowed = FALSE;

    vigs_output->drm = drm;
    vigs_output->num = num;
    vigs_output->mode_connector = connector;
    vigs_output->mode_encoder = encoder;
    vigs_output->dpms_mode = DPMSModeOn;

    return TRUE;
}

void vigs_drm_output_dpms(xf86OutputPtr output, int mode)
{
    struct vigs_drm_output *vigs_output = output->driver_private;
    int i;
    drmModePropertyPtr prop;
    int mode_id = -1;

    VIGS_LOG_TRACE("%d: mode = %d", vigs_output->num, mode);

    for (i = 0; i < vigs_output->mode_connector->count_props; ++i) {
        prop = drmModeGetProperty(vigs_output->drm->fd,
                                  vigs_output->mode_connector->props[i]);

        if (prop) {
            if (prop->flags & DRM_MODE_PROP_ENUM) {
                if (strcmp(prop->name, "DPMS") == 0) {
                    mode_id = vigs_output->mode_connector->props[i];
                    drmModeFreeProperty(prop);
                    break;
                }
            }
            drmModeFreeProperty(prop);
        }
    }

    if (mode_id < 0) {
        VIGS_LOG_WARN("%d: mode = %d - skipped", vigs_output->num, mode);
        return;
    }

    drmModeConnectorSetProperty(vigs_output->drm->fd,
                                vigs_output->mode_connector->connector_id,
                                mode_id, mode);
    vigs_output->dpms_mode = mode;
}
