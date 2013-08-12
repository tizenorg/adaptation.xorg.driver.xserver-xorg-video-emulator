#include "vigs_video_out.h"
#include "vigs_video_adaptor.h"
#include "vigs_video_overlay.h"
#include "vigs_video.h"
#include "vigs_screen.h"
#include "vigs_video_types.h"
#include "vigs_log.h"
#include "vigs_uxa.h"
#include "vigs_pixmap.h"
#include <X11/extensions/Xv.h>

#define VIGS_NUM_PORTS 16

static XF86VideoEncodingRec g_encodings_template[2] =
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
    { 0, -1, 270, "_USER_WM_PORT_ATTRIBUTE_ROTATION" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_HFLIP" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_VFLIP" },
    { 0, -1, 1, "_USER_WM_PORT_ATTRIBUTE_PREEMPTION" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_DRAWING_MODE" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF" },
};

static XF86ImageRec g_images[] =
{
    XVIMAGE_I420,
    XVIMAGE_YV12,
    XVIMAGE_YUY2,
    VIGS_XVIMAGE_RGB32,
    VIGS_XVIMAGE_RGB565,
};

typedef enum
{
    vigs_preemption_low = -1,
    vigs_preemption_default = 0,
    vigs_preemption_high = 1,
} vigs_preemption;

typedef enum
{
    vigs_port_mode_init,
    vigs_port_mode_streaming,
    vigs_port_mode_waiting,
} vigs_port_mode;

typedef enum
{
    vigs_paa_rotation = 0,
    vigs_paa_hflip = 1,
    vigs_paa_vflip = 2,
    vigs_paa_preemption = 3,
    vigs_paa_drawingmode = 4,
    vigs_paa_streamoff = 5,
} vigs_paa;

static struct
{
    const char *name;
    Atom atom;
} g_atom_list[] =
{
    { "_USER_WM_PORT_ATTRIBUTE_ROTATION", None },
    { "_USER_WM_PORT_ATTRIBUTE_HFLIP", None },
    { "_USER_WM_PORT_ATTRIBUTE_VFLIP", None },
    { "_USER_WM_PORT_ATTRIBUTE_PREEMPTION", None },
    { "_USER_WM_PORT_ATTRIBUTE_DRAWING_MODE", None },
    { "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF", None },
};

struct vigs_video_out_adaptor;

struct vigs_video_out_port
{
    struct vigs_video_out_adaptor *adaptor;

    /* -1 for none, >=0 - overlay index. */
    int overlay_index;

    /* Rotation in degrees. -1 for none. */
    int rotation;
    int is_hflip;
    int is_vflip;
    vigs_preemption preemption;
    vigs_port_mode mode;

    void *aligned_buffer;
    int aligned_buffer_width;
    int aligned_buffer_size;
};

struct vigs_video_out_adaptor
{
    struct vigs_video_adaptor base;

    struct vigs_video *xv;

    struct vigs_video_out_port *overlay_owners[VIGS_NUM_VIDEO_OVERLAYS];
};

static Atom vigs_video_out_get_port_atom(vigs_paa paa)
{
    if (g_atom_list[paa].atom == None) {
        g_atom_list[paa].atom = MakeAtom(g_atom_list[paa].name,
                                         strlen(g_atom_list[paa].name),
                                         TRUE);
    }

    return g_atom_list[paa].atom;
}

static Bool vigs_video_out_use_overlay(DrawablePtr drawable)
{
    if (drawable->type == DRAWABLE_PIXMAP) {
        return FALSE;
    } else if (drawable->type == DRAWABLE_WINDOW) {
        PropertyPtr property = vigs_get_window_property((WindowPtr)drawable,
                                                        "XV_ON_DRAWABLE");
        if (property && (*(int*)property->data > 0)) {
            return FALSE;
        }
    }

    return TRUE;
}

static XF86ImagePtr vigs_video_out_get_image_info(int fourcc)
{
    unsigned int i;

    for (i = 0; i < sizeof(g_images) / sizeof(g_images[0]); ++i) {
        if (g_images[i].id == fourcc) {
            return &g_images[i];
        }
    }

    return NULL;
};

static void vigs_video_out_intersect_rect(xRectangle *src1,
                                          xRectangle *src2,
                                          xRectangle *dest)
{
    int dest_x, dest_y;
    int dest_x2, dest_y2;

    dest_x = vigs_max(src1->x, src2->x);
    dest_y = vigs_max(src1->y, src2->y);
    dest_x2 = vigs_min(src1->x + src1->width, src2->x + src2->width);
    dest_y2 = vigs_min(src1->y + src1->height, src2->y + src2->height);

    if ((dest_x2 > dest_x) && (dest_y2 > dest_y)) {
        dest->x = dest_x;
        dest->y = dest_y;
        dest->width = dest_x2 - dest_x;
        dest->height = dest_y2 - dest_y;
    } else {
        dest->width = 0;
        dest->height = 0;
    }
}

/*
 * Video port stuff.
 * @{
 */

static void vigs_video_out_port_close_overlay(struct vigs_video_out_port *port)
{
    if (port->overlay_index < 0) {
        return;
    }

    vigs_video_close_overlay(port->adaptor->xv, port->overlay_index);

    port->adaptor->overlay_owners[port->overlay_index] = NULL;

    port->overlay_index = -1;

    port->mode = vigs_port_mode_waiting;
}

static Bool vigs_video_out_port_open_overlay(struct vigs_video_out_port *port)
{
    int i;

    if (port->overlay_index >= 0) {
        return TRUE;
    }

    if (port->preemption == vigs_preemption_low) {
        port->mode = vigs_port_mode_waiting;
        return TRUE;
    }

    i = vigs_video_open_overlay(port->adaptor->xv);

    if (i >= 0) {
        port->mode = vigs_port_mode_streaming;
        port->overlay_index = i;
        port->adaptor->overlay_owners[i] = port;
        return TRUE;
    }

    if (port->preemption == vigs_preemption_default) {
        port->mode = vigs_port_mode_waiting;
        return TRUE;
    }

    for (i = 0; i < VIGS_NUM_VIDEO_OVERLAYS; ++i) {
        struct vigs_video_out_port *owner =
            port->adaptor->overlay_owners[i];

        if (owner && (owner->preemption == vigs_preemption_default)) {
            vigs_video_out_port_close_overlay(owner);

            i = vigs_video_open_overlay(port->adaptor->xv);
            assert(i >= 0);

            if (i < 0) {
                xf86DrvMsg(port->adaptor->xv->screen->scrn->scrnIndex,
                           X_ERROR, "Cannot preempt a port, logic error\n");
                return FALSE;
            }

            port->mode = vigs_port_mode_streaming;
            port->overlay_index = i;
            port->adaptor->overlay_owners[i] = port;

            return TRUE;
        }
    }

    xf86DrvMsg(port->adaptor->xv->screen->scrn->scrnIndex,
               X_ERROR, "Cannot preempt a port, all busy\n");

    return FALSE;
}

static void vigs_video_out_port_get_rotation(struct vigs_video_out_port *port,
                                             int *hw_rotation)
{
    int rotation = 0;

    if (port->rotation >= 0) {
        rotation = port->rotation;
    }

    *hw_rotation = rotation % 360;
}

/*
 * @}
 */

/*
 * Xv callbacks.
 * @{
 */

static int vigs_video_out_query_image_attributes_internal(int id,
                                                          unsigned short *w,
                                                          unsigned short *h,
                                                          int *pitches,
                                                          int *offsets,
                                                          int *lengths)
{
    int size = 0, tmp = 0;

    VIGS_LOG_TRACE("fourcc = 0x%X", id);

    *w = (*w + 1) & ~1;

    if (offsets) {
        offsets[0] = 0;
    }

    switch (id) {
    case VIGS_FOURCC_RGB565:
        size += (*w << 1);
        if (pitches) {
            pitches[0] = size;
        }
        size *= *h;
        if (lengths) {
            lengths[0] = size;
        }
        break;
    case VIGS_FOURCC_RGB24:
        size += (*w << 1) + *w;
        if (pitches) {
            pitches[0] = size;
        }
        size *= *h;
        if (lengths) {
            lengths[0] = size;
        }
        break;
    case VIGS_FOURCC_RGB32:
        size += (*w << 2);
        if (pitches) {
            pitches[0] = size;
        }
        size *= *h;
        if (lengths) {
            lengths[0] = size;
        }
        break;
    case FOURCC_I420:
    case VIGS_FOURCC_S420:
    case FOURCC_YV12:
        *h = (*h + 1) & ~1;
        size = (*w + 3) & ~3;
        if (pitches) {
            pitches[0] = size;
        }

        size *= *h;

        if (offsets) {
            offsets[1] = size;
        }
        if (lengths) {
            lengths[0] = size;
        }

        tmp = ((*w >> 1) + 3) & ~3;

        if (pitches) {
            pitches[1] = pitches[2] = tmp;
        }

        tmp *= (*h >> 1);
        size += tmp;

        if (offsets) {
            offsets[2] = size;
        }
        if (lengths) {
            lengths[1] = tmp;
        }

        size += tmp;
        if (lengths) {
            lengths[2] = tmp;
        }
        break;
    case FOURCC_UYVY:
    case VIGS_FOURCC_SUYV:
    case FOURCC_YUY2:
    case VIGS_FOURCC_ST12:
    case VIGS_FOURCC_SN12:
        size = *w << 1;

        if (pitches) {
            pitches[0] = size;
        }

        size *= *h;
        break;
    case VIGS_FOURCC_NV12:
        size = *w;
        if (pitches) {
            pitches[0] = size;
        }

        size *= *h;
        if (offsets) {
            offsets[1] = size;
        }

        tmp = *w;
        if (pitches) {
            pitches[1] = tmp;
        }

        tmp *= (*h >> 1);
        size += tmp;
        break;
    default:
        return BadIDChoice;
    }

    return size;
}

static int vigs_video_out_put_image_on_drawable(ScrnInfoPtr scrn,
                                                struct vigs_video_out_port *port,
                                                XF86ImagePtr image_info,
                                                unsigned char *buf,
                                                DrawablePtr drawable,
                                                xRectangle *img_rect,
                                                xRectangle *src_rect,
                                                xRectangle *dst_rect,
                                                RegionPtr clip_boxes,
                                                int hw_rotation)
{
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    pixman_format_code_t src_format, dst_format;
    xRectangle pxm_rect = {0, 0, 0, 0};
    PixmapPtr pixmap = vigs_get_drawable_pixmap(drawable);
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    pxm_rect.width = vigs_pixmap_width(pixmap);
    pxm_rect.height = vigs_pixmap_height(pixmap);

    switch (image_info->id) {
    case FOURCC_I420:
    case FOURCC_YV12:
        src_format = PIXMAN_yv12;
        break;
    case FOURCC_YUY2:
        src_format = PIXMAN_yuy2;
        break;
    case VIGS_FOURCC_RGB565:
        src_format = PIXMAN_r5g6b5;
        break;
    case VIGS_FOURCC_RGB32:
        src_format = PIXMAN_a8r8g8b8;
        break;
    default:
        assert(0);
        return BadRequest;
    }

    /*
     * Since the only supported bpp is 32 we can just convert to RGBX. Though
     * depth can vary, we don't care, pixman will handle it.
     */

    switch (image_info->id) {
    case FOURCC_I420:
        dst_format = PIXMAN_x8b8g8r8;
        break;
    default:
        dst_format = PIXMAN_x8r8g8b8;
        break;
    }

    if ((image_info->id == FOURCC_I420) &&
        ((img_rect->width % 16) != 0)) {
        int src_p[3] = {0,}, src_o[3] = {0,}, src_l[3] = {0,};
        int dst_p[3] = {0,}, dst_o[3] = {0,}, dst_l[3] = {0,};
        unsigned short src_w, src_h, dst_w, dst_h;
        int size;

        src_w = img_rect->width;
        src_h = img_rect->height;

        vigs_video_out_query_image_attributes_internal(image_info->id,
                                                       &src_w, &src_h,
                                                       src_p, src_o, src_l);

        dst_w = (img_rect->width + 15) & ~15;
        dst_h = img_rect->height;

        size = vigs_video_out_query_image_attributes_internal(image_info->id,
                                                              &dst_w, &dst_h,
                                                              dst_p, dst_o,
                                                              dst_l);

        if (!port->aligned_buffer || (port->aligned_buffer_size != size)) {
            free(port->aligned_buffer);

            port->aligned_buffer_size = size;
            port->aligned_buffer = malloc(port->aligned_buffer_size);

            if (!port->aligned_buffer) {
                xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                           "Unable to allocate %d bytes\n", size);
                return BadRequest;
            }
        }

        vigs_copy_image(src_w, src_h,
                        (char*)buf, src_w, src_h,
                        src_p, src_o, src_l,
                        (char*)port->aligned_buffer, dst_w, dst_h,
                        dst_p, dst_o, dst_l,
                        3, 2, 2);

        port->aligned_buffer_width = dst_w;
        img_rect->width = dst_w;
        buf = port->aligned_buffer;
    }

    if (!vigs_screen->no_accel &&
        vigs_pixmap &&
        !vigs_uxa_raw_access(vigs_pixmap,
                             dst_rect->x, dst_rect->y,
                             dst_rect->width, dst_rect->height,
                             vigs_uxa_access_write))
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to raw access drawable %p\n",
                   pixmap);
        return BadRequest;
    }

    vigs_pixman_convert_image(PIXMAN_OP_SRC,
                              buf, pixmap->devPrivate.ptr,
                              src_format, dst_format,
                              img_rect, &pxm_rect, src_rect, dst_rect,
                              NULL, hw_rotation,
                              port->is_hflip, port->is_vflip);

    if (!vigs_screen->no_accel && vigs_pixmap) {
        vigs_uxa_end_raw_access(vigs_pixmap);
    }

    DamageDamageRegion(drawable, clip_boxes);

    return Success;
}

static int vigs_video_out_put_image_on_overlay(ScrnInfoPtr scrn,
                                               struct vigs_video_out_port *port,
                                               XF86ImagePtr image_info,
                                               unsigned char *buf,
                                               xRectangle *img_rect,
                                               xRectangle *src_rect,
                                               xRectangle *dst_rect,
                                               int hw_rotation)
{
    struct vigs_video *xv = port->adaptor->xv;
    pixman_format_code_t src_format, dst_format;
    xRectangle pxm_rect = *dst_rect;
    unsigned char *dst;

    if ((img_rect->width % 16) != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Image width is not multiple of 16\n");
        return BadRequest;
    }

    if (img_rect->width < 16) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Image width is less than 16\n");
        return BadRequest;
    }

    if (img_rect->height < 16) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Image height is less than 16\n");
        return BadRequest;
    }

    switch (image_info->id) {
    case FOURCC_I420:
    case FOURCC_YV12:
        if ((img_rect->height % 2) != 0) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Image height is not multiple of 2\n");
            return BadRequest;
        }
        src_rect->x = src_rect->x & (~0x1);
        src_rect->width = src_rect->width & (~0x1);
        src_rect->height = src_rect->height & (~0x1);
        src_format = PIXMAN_yv12;
        break;
    case FOURCC_YUY2:
        src_rect->x = src_rect->x & (~0x1);
        src_rect->width = src_rect->width & (~0x1);
        src_format = PIXMAN_yuy2;
        break;
    case VIGS_FOURCC_RGB565:
        src_format = PIXMAN_r5g6b5;
        break;
    case VIGS_FOURCC_RGB32:
        src_format = PIXMAN_a8r8g8b8;
        break;
    default:
        assert(0);
        return BadRequest;
    }

    switch (image_info->id) {
    case FOURCC_I420:
        dst_format = PIXMAN_x8b8g8r8;
        break;
    default:
        dst_format = PIXMAN_x8r8g8b8;
        break;
    }

    dst_rect->width = dst_rect->width & (~0x1);
    dst_rect->height = dst_rect->height & (~0x1);

    if (src_rect->width < 16) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Source width is less than 16\n");
        return BadRequest;
    }

    if (src_rect->height < 16) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Source height is less than 16\n");
        return BadRequest;
    }

    if (dst_rect->width < 8) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Overlay viewport width is less than 16\n");
        return BadRequest;
    }

    if (dst_rect->height < 8) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Overlay viewport height is less than 16\n");
        return BadRequest;
    }

    if (!vigs_video_out_port_open_overlay(port)) {
        return BadRequest;
    }

    if (port->mode != vigs_port_mode_streaming) {
        return Success;
    }

    if ((memcmp(vigs_video_overlay_viewport(xv->overlays[port->overlay_index]),
                dst_rect, sizeof(*dst_rect)) != 0)) {
        vigs_video_overlay_stream_off(xv->overlays[port->overlay_index]);
        if (!vigs_video_overlay_set_viewport(xv->overlays[port->overlay_index],
                                             dst_rect)) {
            vigs_video_out_port_close_overlay(port);
            return Success;
        }
    }

    dst = vigs_video_overlay_ptr(xv->overlays[port->overlay_index]);

    if (!dst) {
        vigs_video_out_port_close_overlay(port);
        return Success;
    }

    pxm_rect.x = pxm_rect.y = dst_rect->x = dst_rect->y = 0;

    vigs_pixman_convert_image(PIXMAN_OP_SRC,
                              buf, dst,
                              src_format, dst_format,
                              img_rect, &pxm_rect, src_rect, dst_rect,
                              NULL, hw_rotation,
                              port->is_hflip, port->is_vflip);

    if (!vigs_video_overlay_stream_on(xv->overlays[port->overlay_index])) {
        vigs_video_out_port_close_overlay(port);
    }

    return Success;
}

static int vigs_video_out_put_image(ScrnInfoPtr scrn,
                                    short src_x, short src_y,
                                    short dst_x, short dst_y,
                                    short src_w, short src_h,
                                    short dst_w, short dst_h,
                                    int id,
                                    unsigned char *buf,
                                    short width, short height,
                                    Bool sync,
                                    RegionPtr clip_boxes,
                                    pointer data,
                                    DrawablePtr drawable)
{
    struct vigs_video_out_port *port = data;
    XF86ImagePtr image_info = NULL;
    int hw_rotation = 0;
    xRectangle img_rect = {0, 0, width, height};
    xRectangle src_rect = {src_x, src_y, src_w, src_h};
    xRectangle dst_rect = {dst_x, dst_y, dst_w, dst_h};
    Bool use_overlay;

    image_info = vigs_video_out_get_image_info(id);

    if (!image_info) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "fourcc 0x%X not supported\n", id);
        return BadRequest;
    }

    vigs_video_out_intersect_rect(&src_rect, &img_rect, &src_rect);

    vigs_video_out_port_get_rotation(port, &hw_rotation);

    use_overlay = vigs_video_out_use_overlay(drawable);

    VIGS_LOG_TRACE("overlay = %d, src = %d,%d %dx%d, dst = %d,%d %dx%d, fourcc = 0x%X, %dx%d, drawable = %p, hw_rotation = %d, hflip = %d, vflip = %d",
                   use_overlay,
                   (int)src_x, (int)src_y, (int)src_w, (int)src_h,
                   (int)dst_x, (int)dst_y, (int)dst_w, (int)dst_h,
                   id, (int)width, (int)height, drawable, hw_rotation,
                   port->is_hflip, port->is_vflip);

    if (use_overlay) {
        /*
         * Currently it's not used in Tizen.
         *
         * TODO: Test this.
         */
        return vigs_video_out_put_image_on_overlay(scrn,
                                                   port,
                                                   image_info,
                                                   buf,
                                                   &img_rect,
                                                   &src_rect,
                                                   &dst_rect,
                                                   hw_rotation);
    } else {
        return vigs_video_out_put_image_on_drawable(scrn,
                                                    port,
                                                    image_info,
                                                    buf,
                                                    drawable,
                                                    &img_rect,
                                                    &src_rect,
                                                    &dst_rect,
                                                    clip_boxes,
                                                    hw_rotation);
    }
}

#if (GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) >= 13)
static int vigs_video_out_reput_image(ScrnInfoPtr scrn,
                                      short src_x, short src_y,
                                      short drw_x, short drw_y, short src_w,
                                      short src_h, short drw_w, short drw_h,
                                      RegionPtr clip_boxes,
                                      pointer data,
                                      DrawablePtr drawable)
#else
static int vigs_video_out_reput_image(ScrnInfoPtr scrn,
                                      short drw_x, short drw_y,
                                      RegionPtr clip_boxes,
                                      pointer data,
                                      DrawablePtr drawable)
#endif
{
    VIGS_LOG_TRACE("enter");

    return Success;
}

static void vigs_video_out_stop_video(ScrnInfoPtr scrn,
                                      pointer data,
                                      Bool exit)
{
    struct vigs_video_out_port *port = data;

    VIGS_LOG_TRACE("enter");

    vigs_video_out_port_close_overlay(port);

    port->rotation = -1;
    port->preemption = vigs_preemption_default;
    port->mode = vigs_port_mode_init;
}

static int vigs_video_out_get_port_attribute(ScrnInfoPtr scrn,
                                             Atom attribute,
                                             INT32 *value,
                                             pointer data)
{
    struct vigs_video_out_port *port = data;

    VIGS_LOG_TRACE("attribute = %lu", attribute);

    if (attribute == vigs_video_out_get_port_atom(vigs_paa_rotation)) {
        *value = port->rotation;
        return Success;
    } else if (attribute == vigs_video_out_get_port_atom(vigs_paa_hflip)) {
        *value = port->is_hflip;
        return Success;
    } else if (attribute == vigs_video_out_get_port_atom(vigs_paa_vflip)) {
        *value = port->is_vflip;
        return Success;
    } else if (attribute == vigs_video_out_get_port_atom(vigs_paa_preemption)) {
        *value = port->preemption;
        return Success;
    } else if (attribute == vigs_video_out_get_port_atom(vigs_paa_drawingmode)) {
        *value = (port->mode == vigs_port_mode_waiting);
        return Success;
    }

    return BadMatch;
}

static int vigs_video_out_set_port_attribute(ScrnInfoPtr scrn,
                                             Atom attribute,
                                             INT32 value,
                                             pointer data)
{
    struct vigs_video_out_port *port = data;

    if (attribute == vigs_video_out_get_port_atom(vigs_paa_rotation)) {
        VIGS_LOG_TRACE("rotation value = %ld", value);
        port->rotation = value;
        return Success;
    } else if (attribute == vigs_video_out_get_port_atom(vigs_paa_hflip)) {
        VIGS_LOG_TRACE("hflip value = %ld", value);
        port->is_hflip = value;
        return Success;
    } else if (attribute == vigs_video_out_get_port_atom(vigs_paa_vflip)) {
        VIGS_LOG_TRACE("vflip value = %ld", value);
        port->is_vflip = value;
        return Success;
    } else if (attribute == vigs_video_out_get_port_atom(vigs_paa_preemption)) {
        VIGS_LOG_TRACE("preemption value = %ld", value);
        switch (value) {
        case vigs_preemption_low:
        case vigs_preemption_default:
        case vigs_preemption_high:
            port->preemption = value;
            break;
        default:
            xf86DrvMsg(scrn->scrnIndex, X_WARNING, "Bad value %ld for port attribute \"preemption\"\n", value);
            port->preemption = vigs_preemption_default;
            break;
        }
        return Success;
    } else if (attribute == vigs_video_out_get_port_atom(vigs_paa_streamoff)) {
        VIGS_LOG_TRACE("streamoff value = %ld", value);
        vigs_video_out_port_close_overlay(port);
        return Success;
    } else {
        VIGS_LOG_ERROR("unknown attribute %lu", attribute);
    }

    return BadMatch;
}

static void vigs_video_out_query_best_size(ScrnInfoPtr scrn,
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

static int vigs_video_out_query_image_attributes(ScrnInfoPtr scrn,
                                                 int id,
                                                 unsigned short *w,
                                                 unsigned short *h,
                                                 int *pitches,
                                                 int *offsets)
{
    return vigs_video_out_query_image_attributes_internal(id,
                                                          w,
                                                          h,
                                                          pitches,
                                                          offsets,
                                                          NULL);
}

/*
 * @}
 */

static void vigs_video_out_destroy(struct vigs_video_adaptor *adaptor)
{
    struct vigs_video_out_adaptor *out_adaptor = (struct vigs_video_out_adaptor*)adaptor;
    int i;

    VIGS_LOG_TRACE("enter");

    for (i = 0; i < VIGS_NUM_PORTS; ++i) {
        struct vigs_video_out_port* port =
            (struct vigs_video_out_port*)adaptor->base.pPortPrivates[i].ptr;

        vigs_video_out_port_close_overlay(port);

        free(port->aligned_buffer);
    }

    free(out_adaptor);
}

struct vigs_video_adaptor *vigs_video_out_create(struct vigs_video *xv)
{
    ScrnInfoPtr scrn = xv->screen->scrn;
    struct vigs_video_out_adaptor *adaptor;
    XF86VideoEncodingRec encodings[2];
    struct vigs_video_out_port *ports;
    int i;

    VIGS_LOG_TRACE("enter");

    adaptor = calloc(1,
        sizeof(*adaptor) +
        (sizeof(DevUnion) + sizeof(struct vigs_video_out_port)) * VIGS_NUM_PORTS);

    if (!adaptor) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate video output adaptor\n");
        return NULL;
    }

    memcpy(&encodings[0], &g_encodings_template[0], sizeof(encodings));

    encodings[0].width = scrn->pScreen->width;
    encodings[0].height = scrn->pScreen->height;

    adaptor->base.base.type = XvWindowMask | XvPixmapMask | XvInputMask | XvImageMask;
    adaptor->base.base.flags = (VIDEO_CLIP_TO_VIEWPORT | VIDEO_OVERLAID_IMAGES);
    adaptor->base.base.name = "VIGS Video Output Adaptor";
    adaptor->base.base.nEncodings = sizeof(encodings) / sizeof(encodings[0]);
    adaptor->base.base.pEncodings = encodings;
    adaptor->base.base.nFormats = sizeof(g_formats) / sizeof(g_formats[0]);
    adaptor->base.base.pFormats = g_formats;
    adaptor->base.base.nPorts = VIGS_NUM_PORTS;
    adaptor->base.base.pPortPrivates = (DevUnion*)(&adaptor[1]);

    ports = (struct vigs_video_out_port*)(&adaptor->base.base.pPortPrivates[VIGS_NUM_PORTS]);

    for (i = 0; i < VIGS_NUM_PORTS; ++i) {
        adaptor->base.base.pPortPrivates[i].ptr = &ports[i];

        ports[i].adaptor = adaptor;
        ports[i].overlay_index = -1;
        ports[i].rotation = -1;
        ports[i].preemption = vigs_preemption_default;
        ports[i].mode = vigs_port_mode_init;
    }

    adaptor->base.base.nAttributes = sizeof(g_attributes) / sizeof(g_attributes[0]);
    adaptor->base.base.pAttributes = g_attributes;
    adaptor->base.base.nImages = sizeof(g_images) / sizeof(g_images[0]);
    adaptor->base.base.pImages = g_images;

    adaptor->base.base.PutImage = &vigs_video_out_put_image;
    adaptor->base.base.ReputImage = &vigs_video_out_reput_image;
    adaptor->base.base.StopVideo = &vigs_video_out_stop_video;
    adaptor->base.base.GetPortAttribute = &vigs_video_out_get_port_attribute;
    adaptor->base.base.SetPortAttribute = &vigs_video_out_set_port_attribute;
    adaptor->base.base.QueryBestSize = &vigs_video_out_query_best_size;
    adaptor->base.base.QueryImageAttributes = &vigs_video_out_query_image_attributes;

    adaptor->base.destroy = &vigs_video_out_destroy;

    adaptor->xv = xv;

    return &adaptor->base;
}
