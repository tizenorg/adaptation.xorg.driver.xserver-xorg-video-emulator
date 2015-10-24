/*
 * X.Org X server driver for VIGS
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact :
 * Vasily Ulyanov <v.ulyanov@samsung.com>
 * Jinhyung Jo <jinhyung.jo@samsung.com>
 * Sangho Park <sangho.p@samsung.com>
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

#include "vigs_xv_still_out.h"
#include "vigs_xv_adaptor.h"
#include "vigs_xv.h"
#include "vigs_screen.h"
#include "vigs_xv_types.h"
#include "vigs_log.h"
#include "vigs_uxa.h"
#include "vigs_pixmap.h"
#include <X11/extensions/Xv.h>
#include "vigs.h"

#define VIGS_NUM_PORTS 1

typedef enum
{
    vigs_capture_mode_none,
    vigs_capture_mode_still,
    vigs_capture_mode_stream,
    vigs_capture_mode_max,
} vigs_capture_mode;

typedef enum
{
    vigs_data_type_none,
    vigs_data_type_ui,
    vigs_data_type_video,
    vigs_data_type_max,
} vigs_data_type;

static XF86VideoEncodingRec g_encodings[] =
{
    { 0, "XV_IMAGE", -1, -1, { 1, 1 } },
    { 1, "XV_IMAGE", 2560, 2560, { 1, 1 } },
};

static XF86VideoFormatRec g_formats[] =
{
    { 16, TrueColor },
    { 24, TrueColor },
    { 32, TrueColor },
};

static XF86AttributeRec g_attributes[] =
{
    { 0, 0, 0x7fffffff, "_USER_WM_PORT_ATTRIBUTE_FORMAT" },
    { 0, 0, vigs_capture_mode_max, "_USER_WM_PORT_ATTRIBUTE_CAPTURE" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_DISPLAY" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_ROTATE_OFF" },
    { 0, 0, vigs_data_type_max, "_USER_WM_PORT_ATTRIBUTE_DATA_TYPE" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_SECURE" },
    { 0, 0, 0x7fffffff, "_USER_WM_PORT_ATTRIBUTE_RETURN_BUFFER" },
};

static XF86ImageRec g_images[] =
{
    VIGS_XVIMAGE_RGB32,
};

typedef enum
{
    vigs_paa_format = 0,
    vigs_paa_capture = 1,
    vigs_paa_display = 2,
    vigs_paa_rotate_off = 3,
    vigs_paa_data_type = 4,
    vigs_paa_secure = 5,
    vigs_paa_retbuf = 6,
} vigs_paa;

static struct
{
    const char *name;
    Atom atom;
} g_atom_list[] =
{
    { "_USER_WM_PORT_ATTRIBUTE_FORMAT", None },
    { "_USER_WM_PORT_ATTRIBUTE_CAPTURE", None },
    { "_USER_WM_PORT_ATTRIBUTE_DISPLAY", None },
    { "_USER_WM_PORT_ATTRIBUTE_ROTATE_OFF", None },
    { "_USER_WM_PORT_ATTRIBUTE_DATA_TYPE", None },
    { "_USER_WM_PORT_ATTRIBUTE_SECURE", None },
    { "_USER_WM_PORT_ATTRIBUTE_RETURN_BUFFER", None },
};

struct vigs_xv_still_out_port
{
    struct vigs_xv_still_out_adaptor *adaptor;

    /* port attributes */
    int format;
    int display;
    bool rotate_off;
    bool secure;
    vigs_capture_mode capture_mode;
    vigs_data_type data_type;
};

struct vigs_xv_still_out_adaptor
{
    struct vigs_xv_adaptor base;

    struct vigs_xv *xv;
};

static Atom vigs_xv_still_out_get_port_atom(vigs_paa paa)
{
    if (g_atom_list[paa].atom == None) {
        g_atom_list[paa].atom = MakeAtom(g_atom_list[paa].name,
                                         strlen(g_atom_list[paa].name),
                                         TRUE);
    }

    return g_atom_list[paa].atom;
}

static XF86ImagePtr vigs_xv_still_out_get_image_info(int fourcc)
{
    unsigned int i;

    for (i = 0; i < sizeof(g_images) / sizeof(g_images[0]); ++i) {
        if (g_images[i].id == fourcc) {
            return &g_images[i];
        }
    }

    return NULL;
};

/*
 * Xv callbacks.
 * @{
 */

static int vigs_xv_still_out_get_port_attribute(ScrnInfoPtr scrn,
                                                Atom attribute,
                                                INT32 *value,
                                                pointer data)
{
    struct vigs_xv_still_out_port *port = data;

    VIGS_LOG_TRACE("attribute = %lu", attribute);

    if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_format)) {
        *value = port->format;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_capture)) {
        *value = port->capture_mode;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_display)) {
        *value = port->display;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_rotate_off)) {
        *value = port->rotate_off;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_data_type)) {
        *value = port->data_type;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_secure)) {
        *value = port->secure;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_retbuf)) {
        *value = 0; /* unused */
        return Success;
    }

    return BadMatch;
}

static int vigs_xv_still_out_set_port_attribute(ScrnInfoPtr scrn,
                                                Atom attribute,
                                                INT32 value,
                                                pointer data)
{
    struct vigs_xv_still_out_port *port = data;

    if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_format)) {
        VIGS_LOG_TRACE("format value = %ld", value);
        if (vigs_xv_still_out_get_image_info(value)) {
            port->format = value;
            return Success;
        }
        xf86DrvMsg(scrn->scrnIndex, X_WARNING, "Format %ld not supported\n", value);
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_capture)) {
        VIGS_LOG_TRACE("capture value = %ld", value);
        switch (value) {
        case vigs_capture_mode_none:
        case vigs_capture_mode_still:
        case vigs_capture_mode_stream:
            port->capture_mode = value;
            break;
        default:
            xf86DrvMsg(scrn->scrnIndex, X_WARNING,
                       "Bad value %ld for port attribute \"capture\"\n",
                       value);
            return BadRequest;
        }
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_display)) {
        VIGS_LOG_TRACE("display value = %ld", value);
        port->display = value;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_rotate_off)) {
        VIGS_LOG_TRACE("rotate_off value = %ld", value);
        port->rotate_off = value;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_data_type)) {
        VIGS_LOG_TRACE("data_type value = %ld", value);
        switch (value) {
        case vigs_data_type_none:
        case vigs_data_type_ui:
        case vigs_data_type_video:
            port->data_type = value;
            break;
        default:
            xf86DrvMsg(scrn->scrnIndex, X_WARNING,
                       "Bad value %ld for port attribute \"data_type\"\n",
                       value);
            return BadRequest;
        }
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_secure)) {
        VIGS_LOG_TRACE("secure value = %ld", value);
        port->secure = value;
        return Success;
    } else if (attribute == vigs_xv_still_out_get_port_atom(vigs_paa_retbuf)) {
        VIGS_LOG_TRACE("retbuf value = %ld", value);
        return Success; /* unused */
    } else {
        VIGS_LOG_ERROR("unknown attribute %lu", attribute);
    }

    return BadMatch;
}

static void vigs_xv_still_out_query_best_size(ScrnInfoPtr scrn,
                                              Bool motion,
                                              short vid_w, short vid_h,
                                              short dst_w, short dst_h,
                                              unsigned int *p_w, unsigned int *p_h,
                                              pointer data)
{
    VIGS_LOG_TRACE("motion = %d, vid = %dx%d, dst = %dx%d",
                   motion, (int)vid_w, (int)vid_h, (int)dst_w, (int)dst_h);

    *p_w = dst_w;
    *p_h = dst_h;
}

static int vigs_xv_still_out_put_still(ScrnInfoPtr scrn,
                                       short src_x, short src_y,
                                       short dst_x, short dst_y,
                                       short src_w, short src_h,
                                       short dst_w, short dst_h,
                                       RegionPtr clip_boxes,
                                       pointer data,
                                       DrawablePtr drawable)
{
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_drm_surface *front_sfc = vigs_screen->front_sfc;
    struct vigs_xv_still_out_port *port = data;
    PixmapPtr pixmap = vigs_get_drawable_pixmap(drawable);
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    xRectangle src_rect = {src_x, src_y, src_w, src_h};
    xRectangle dst_rect = {dst_x, dst_y, dst_w, dst_h};
    xRectangle img_rect = {0, 0, front_sfc->width, front_sfc->height};
    xRectangle pxm_rect = {0, 0, vigs_pixmap_width(pixmap), vigs_pixmap_height(pixmap)};
    pixman_format_code_t src_format, dst_format;
    int ret = Success;

    VIGS_LOG_TRACE("%p: src = %d,%d %dx%d, dst = %d,%d %dx%d, drawable = %p",
                   front_sfc->gem.vaddr,
                   (int)src_x, (int)src_y, (int)src_w, (int)src_h,
                   (int)dst_x, (int)dst_y, (int)dst_w, (int)dst_h,
                   drawable);

    src_format = PIXMAN_a8r8g8b8;
    dst_format = PIXMAN_x8r8g8b8;

    if (vigs_drm_gem_map(front_sfc, 1) != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to map GEM: %s\n", strerror(-ret));
        return BadRequest;
    }

    if (vigs_drm_surface_start_access(front_sfc, VIGS_DRM_SAF_READ) != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to start GEM access: %s\n", strerror(-ret));
        return BadRequest;
    }

    if (!vigs_screen->no_accel &&
        vigs_pixmap &&
        !vigs_uxa_raw_access(vigs_pixmap,
                             dst_rect.x, dst_rect.y,
                             dst_rect.width, dst_rect.height,
                             vigs_uxa_access_write))
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to raw access drawable %p\n", pixmap);
        ret = BadRequest;
        goto out_end_access;
    }

    vigs_pixman_convert_image(PIXMAN_OP_SRC,
                              front_sfc->gem.vaddr,
                              pixmap->devPrivate.ptr,
                              src_format,
                              dst_format,
                              &img_rect,
                              &pxm_rect,
                              &src_rect,
                              &dst_rect,
                              clip_boxes,
                              0 /* rotation */,
                              0 /* hflip */,
                              0 /* vflip */);

    if (!vigs_screen->no_accel && vigs_pixmap) {
        vigs_uxa_end_raw_access(vigs_pixmap);
    }

out_end_access:
    if (vigs_drm_surface_end_access(front_sfc, 0) != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to end GEM access: %s\n", strerror(-ret));
    }

    if (ret == Success) {
        DamageDamageRegion(drawable, clip_boxes);
    }

    return ret;
}

static void vigs_xv_still_out_stop_video(ScrnInfoPtr scrn,
                                         pointer data,
                                         Bool exit)
{
    VIGS_LOG_TRACE("exit = %d", (int)exit);
}

/*
 * @}
 */

static void vigs_xv_still_out_destroy(struct vigs_xv_adaptor *adaptor)
{
    struct vigs_xv_still_out_adaptor *out_adaptor = (struct vigs_xv_still_out_adaptor*)adaptor;
    int i;

    VIGS_LOG_TRACE("enter");

    for (i = 0; i < VIGS_NUM_PORTS; ++i) {
        struct vigs_xv_still_out_port *port =
            (struct vigs_xv_still_out_port *)adaptor->base.pPortPrivates[i].ptr;
    }

    free(out_adaptor);
}

struct vigs_xv_adaptor *vigs_xv_still_out_create(struct vigs_xv *xv)
{
    ScrnInfoPtr scrn = xv->screen->scrn;
    struct vigs_xv_still_out_adaptor *adaptor;
    struct vigs_xv_still_out_port *ports;
    int i;

    VIGS_LOG_TRACE("enter");

    adaptor = calloc(1,
        sizeof(*adaptor) +
        (sizeof(DevUnion) + sizeof(struct vigs_xv_still_out_port)) * VIGS_NUM_PORTS);

    if (!adaptor) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate still output adaptor\n");
        return NULL;
    }

    g_encodings[0].width = scrn->pScreen->width;
    g_encodings[0].height = scrn->pScreen->height;

    adaptor->base.base.type = XvWindowMask | XvPixmapMask | XvInputMask | XvStillMask;
    adaptor->base.base.flags = 0;
    adaptor->base.base.name = "VIGS Still Output Adaptor";
    adaptor->base.base.nEncodings = sizeof(g_encodings) / sizeof(g_encodings[0]);
    adaptor->base.base.pEncodings = g_encodings;
    adaptor->base.base.nFormats = sizeof(g_formats) / sizeof(g_formats[0]);
    adaptor->base.base.pFormats = g_formats;
    adaptor->base.base.nPorts = VIGS_NUM_PORTS;
    adaptor->base.base.pPortPrivates = (DevUnion *)(&adaptor[1]);

    ports = (struct vigs_xv_still_out_port*)(&adaptor->base.base.pPortPrivates[VIGS_NUM_PORTS]);

    for (i = 0; i < VIGS_NUM_PORTS; ++i) {
        adaptor->base.base.pPortPrivates[i].ptr = &ports[i];

        ports[i].adaptor = adaptor;
        ports[i].format = VIGS_FOURCC_RGB32;
    }

    adaptor->base.base.nAttributes = sizeof(g_attributes) / sizeof(g_attributes[0]);
    adaptor->base.base.pAttributes = g_attributes;
    adaptor->base.base.nImages = sizeof(g_images) / sizeof(g_images[0]);
    adaptor->base.base.pImages = g_images;

    adaptor->base.base.GetPortAttribute = &vigs_xv_still_out_get_port_attribute;
    adaptor->base.base.SetPortAttribute = &vigs_xv_still_out_set_port_attribute;
    adaptor->base.base.QueryBestSize = &vigs_xv_still_out_query_best_size;
    adaptor->base.base.PutStill = &vigs_xv_still_out_put_still;
    adaptor->base.base.StopVideo = &vigs_xv_still_out_stop_video;

    adaptor->base.destroy = &vigs_xv_still_out_destroy;

    adaptor->xv = xv;

    return &adaptor->base;
}
