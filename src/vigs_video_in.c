#include "vigs_video_in.h"
#include "vigs_video_adaptor.h"
#include "vigs_video.h"
#include "vigs_screen.h"
#include "vigs_video_types.h"
#include "vigs_log.h"
#include <X11/extensions/Xv.h>

#define VIGS_NUM_PORTS 1

typedef enum
{
    vigs_data_type_ui = 0,
    vigs_data_type_video = 1,
} vigs_data_type;

#define VIGS_NUM_DATA_TYPES 2

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
    { 0, 0, 0x7fffffff, "_USER_WM_PORT_ATTRIBUTE_FORMAT" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_CAPTURE" },
    { 0, 0, VIGS_NUM_DATA_TYPES, "_USER_WM_PORT_ATTRIBUTE_DATA_TYPE" },
    { 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF" },
};

static XF86ImageRec g_images[] =
{
    VIGS_XVIMAGE_RGB32,
    VIGS_XVIMAGE_SN12,
    VIGS_XVIMAGE_ST12,
};

typedef enum
{
    vigs_paa_format = 0,
    vigs_paa_capture = 1,
    vigs_paa_data_type = 2,
    vigs_paa_streamoff = 3,
} vigs_paa;

static const struct
{
    const char *name;
    Atom atom;
} g_atom_list[] =
{
    { "_USER_WM_PORT_ATTRIBUTE_FORMAT", None },
    { "_USER_WM_PORT_ATTRIBUTE_CAPTURE", None },
    { "_USER_WM_PORT_ATTRIBUTE_DATA_TYPE", None },
    { "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF", None },
};

static RESTYPE g_resource_type = 0;

struct vigs_video_in_adaptor;

struct vigs_video_in_port
{
    struct vigs_video_in_adaptor *adaptor;
};

struct vigs_video_in_resource
{
    XID id;

    RESTYPE type;

    struct vigs_video_in_port *port;
};

struct vigs_video_in_adaptor
{
    struct vigs_video_adaptor base;

    struct vigs_video *xv;
};

static int vigs_video_in_get_port_attribute(ScrnInfoPtr scrn,
                                            Atom attribute,
                                            INT32 *value,
                                            pointer data)
{
    VIGS_LOG_TRACE("attribute = %lu", attribute);

    return BadMatch;
}

static int vigs_video_in_set_port_attribute(ScrnInfoPtr scrn,
                                            Atom attribute,
                                            INT32 value,
                                            pointer data)
{
    VIGS_LOG_TRACE("attribute = %lu, value = %d", attribute, value);

    return BadMatch;
}

static void vigs_video_in_query_best_size(ScrnInfoPtr scrn,
                                          Bool motion,
                                          short vid_w, short vid_h,
                                          short dst_w, short dst_h,
                                          unsigned int *p_w, unsigned int *p_h,
                                          pointer data)
{
    VIGS_LOG_TRACE("motion = %d, vid = %dx%d, dst = %dx%d",
                   motion, (int)vid_w, (int)vid_h, (int)dst_w, (int)dst_h);
}

static int vigs_video_in_get_still(ScrnInfoPtr scrn,
                                   short vid_x, short vid_y,
                                   short drw_x, short drw_y,
                                   short vid_w, short vid_h,
                                   short drw_w, short drw_h,
                                   RegionPtr clip_boxes,
                                   pointer data,
                                   DrawablePtr drawable)
{
    VIGS_LOG_TRACE("vid = %d,%d %dx%d, drw = %d,%d %dx%d, drawable = %p",
                   (int)vid_x, (int)vid_y, (int)vid_w, (int)vid_h,
                   (int)drw_x, (int)drw_y, (int)drw_w, (int)drw_h,
                   drawable);

    return Success;
}

static void vigs_video_in_stop_video(ScrnInfoPtr scrn,
                                     pointer data,
                                     Bool exit)
{
    VIGS_LOG_TRACE("enter");
}

static int vigs_video_in_resource_destroy(pointer data, XID id)
{
    struct vigs_video_in_resource *res = (struct vigs_video_in_resource*)data;

    VIGS_LOG_TRACE("id = 0x%X", id);

    if (!res) {
        return Success;
    }

    free(res);

    return Success;
}

static void vigs_video_in_destroy(struct vigs_video_adaptor *adaptor)
{
    struct vigs_video_in_adaptor *in_adaptor = (struct vigs_video_in_adaptor*)adaptor;

    VIGS_LOG_TRACE("enter");

    free(in_adaptor);
}

struct vigs_video_adaptor *vigs_video_in_create(struct vigs_video *xv)
{
    ScrnInfoPtr scrn = xv->screen->scrn;
    struct vigs_video_in_adaptor *adaptor;
    XF86VideoEncodingRec encodings[2];
    struct vigs_video_in_port *ports;
    int i;

    VIGS_LOG_TRACE("enter");

    adaptor = calloc(1,
        sizeof(*adaptor) +
        (sizeof(DevUnion) + sizeof(struct vigs_video_in_port)) * VIGS_NUM_PORTS);

    if (!adaptor) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate video input adaptor\n");
        return NULL;
    }

    memcpy(&encodings[0], &g_encodings_template[0], sizeof(encodings));

    encodings[0].width = scrn->pScreen->width;
    encodings[0].height = scrn->pScreen->height;

    adaptor->base.base.type = XvWindowMask | XvPixmapMask | XvOutputMask | XvStillMask;
    adaptor->base.base.flags = 0;
    adaptor->base.base.name = "VIGS Video Input Adaptor";
    adaptor->base.base.nEncodings = sizeof(encodings) / sizeof(encodings[0]);
    adaptor->base.base.pEncodings = encodings;
    adaptor->base.base.nFormats = sizeof(g_formats) / sizeof(g_formats[0]);
    adaptor->base.base.pFormats = g_formats;

    adaptor->base.base.nPorts = VIGS_NUM_PORTS;
    adaptor->base.base.pPortPrivates = (DevUnion*)(&adaptor[1]);

    ports = (struct vigs_video_in_port*)(&adaptor->base.base.pPortPrivates[VIGS_NUM_PORTS]);

    for (i = 0; i < VIGS_NUM_PORTS; ++i) {
        adaptor->base.base.pPortPrivates[i].ptr = &ports[i];

        ports[i].adaptor = adaptor;
    }

    adaptor->base.base.nAttributes = sizeof(g_attributes) / sizeof(g_attributes[0]);
    adaptor->base.base.pAttributes = g_attributes;
    adaptor->base.base.nImages = sizeof(g_images) / sizeof(g_images[0]);
    adaptor->base.base.pImages = g_images;

    adaptor->base.base.GetPortAttribute = &vigs_video_in_get_port_attribute;
    adaptor->base.base.SetPortAttribute = &vigs_video_in_set_port_attribute;
    adaptor->base.base.QueryBestSize = &vigs_video_in_query_best_size;
    adaptor->base.base.GetStill = &vigs_video_in_get_still;
    adaptor->base.base.StopVideo = &vigs_video_in_stop_video;

    adaptor->base.destroy = &vigs_video_in_destroy;

    adaptor->xv = xv;

    if (g_resource_type == 0) {
        g_resource_type = CreateNewResourceType(&vigs_video_in_resource_destroy,
                                                "VIGS Video Input Resource");
        if (!g_resource_type) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to create video input resource type\n");
            return NULL;
        }
    }

    return &adaptor->base;
}
