#include "vigs_video_overlay.h"
#include "vigs_screen.h"
#include "vigs_log.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define PAGE_SIZE 4096

static struct
{
    int cap;
    const char *name;
} g_caps[] =
{
    {V4L2_CAP_VIDEO_CAPTURE, "VIDEO_CAPTURE"},
    {V4L2_CAP_VIDEO_OUTPUT, "VIDEO_OUTPUT"},
    {V4L2_CAP_VIDEO_OVERLAY, "VIDEO_OVERLAY"},
    {V4L2_CAP_VBI_CAPTURE, "VBI_CAPTURE"},
    {V4L2_CAP_VBI_OUTPUT, "VBI_OUTPUT"},
    {V4L2_CAP_SLICED_VBI_CAPTURE, "SLICED_VBI_CAPTURE"},
    {V4L2_CAP_SLICED_VBI_OUTPUT, "SLICED_VBI_OUTPUT"},
    {V4L2_CAP_RDS_CAPTURE, "RDS_CAPTURE"},
    {V4L2_CAP_VIDEO_OUTPUT_OVERLAY, "VIDEO_OUTPUT_OVERLAY"},
    {V4L2_CAP_HW_FREQ_SEEK, "HW_FREQ_SEEK"},
    {V4L2_CAP_RDS_OUTPUT, "RDS_OUTPUT"},
    {V4L2_CAP_TUNER, "TUNER"},
    {V4L2_CAP_AUDIO, "AUDIO"},
    {V4L2_CAP_RADIO, "RADIO"},
    {V4L2_CAP_MODULATOR, "MODULATOR"},
    {V4L2_CAP_READWRITE, "READWRITE"},
    {V4L2_CAP_ASYNCIO, "ASYNCIO"},
    {V4L2_CAP_STREAMING, "STREAMING"}
};

static Bool vigs_video_overlay_query_caps(struct vigs_video_overlay *overlay,
                                          int caps)
{
    ScrnInfoPtr scrn = overlay->screen->scrn;
    struct v4l2_capability cap;
    int ret;
    int unsupported;

    memset(&cap, 0, sizeof(cap));

    ret = ioctl(overlay->fd, VIDIOC_QUERYCAP, &cap);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "VIDIOC_QUERYCAP failed for \"%s\": %s\n",
                   overlay->device_name, strerror(errno));
        return FALSE;
    }

    unsupported = ~(cap.capabilities) & caps;

    if (unsupported) {
        unsigned int i;

        for (i = 0; i < sizeof(g_caps) / sizeof(g_caps[0]); ++i) {
            if (unsupported & g_caps[i].cap) {
                xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Video cap \"%s\" is not supported\n",
                           g_caps[i].name);
            }
        }

        return FALSE;
    }

    return TRUE;
}

static Bool vigs_video_overlay_get_format(struct vigs_video_overlay *overlay,
                                          struct v4l2_format *format)
{
    ScrnInfoPtr scrn = overlay->screen->scrn;
    int ret;

    ret = ioctl(overlay->fd, VIDIOC_G_FMT, format);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "VIDIOC_G_FMT failed for \"%s\": %s\n",
                   overlay->device_name, strerror(errno));
        return FALSE;
    }

    return TRUE;
}

static Bool vigs_video_overlay_set_format(struct vigs_video_overlay *overlay,
                                          const struct v4l2_format *format)
{
    ScrnInfoPtr scrn = overlay->screen->scrn;
    int ret;

    ret = ioctl(overlay->fd, VIDIOC_S_FMT, format);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "VIDIOC_S_FMT failed for \"%s\": %s\n",
                   overlay->device_name, strerror(errno));
        return FALSE;
    }

    return TRUE;
}

static Bool vigs_video_overlay_query_buf(struct vigs_video_overlay *overlay,
                                         struct v4l2_buffer *buf)
{
    ScrnInfoPtr scrn = overlay->screen->scrn;
    int ret;

    ret = ioctl(overlay->fd, VIDIOC_QUERYBUF, (void*)buf);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "VIDIOC_QUERYBUF failed for \"%s\": %s\n",
                   overlay->device_name, strerror(errno));
        return FALSE;
    }

    return TRUE;
}

static Bool vigs_video_overlay_start(struct vigs_video_overlay *overlay)
{
    ScrnInfoPtr scrn = overlay->screen->scrn;
    int start = 1;
    int ret;

    ret = ioctl(overlay->fd, VIDIOC_OVERLAY, (void*)&start);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "VIDIOC_OVERLAY start failed for \"%s\": %s\n",
                   overlay->device_name, strerror(errno));
        return FALSE;
    }

    return TRUE;
}

static Bool vigs_video_overlay_stop(struct vigs_video_overlay *overlay)
{
    ScrnInfoPtr scrn = overlay->screen->scrn;
    int start = 0;
    int ret;

    ret = ioctl(overlay->fd, VIDIOC_OVERLAY, (void*)&start);

    if (ret < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "VIDIOC_OVERLAY stop failed for \"%s\": %s\n",
                   overlay->device_name, strerror(errno));
        return FALSE;
    }

    return TRUE;
}

struct vigs_video_overlay
    *vigs_video_overlay_create(struct vigs_screen *screen,
                               const char *device_name)
{
    ScrnInfoPtr scrn = screen->scrn;
    struct vigs_video_overlay *overlay;

    VIGS_LOG_TRACE("device_name = %s", device_name);

    overlay = calloc(1, sizeof(*overlay));

    if (!overlay) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate overlay for \"%s\"\n", device_name);
        return NULL;
    }

    overlay->screen = screen;
    overlay->device_name = strdup(device_name);

    if (!overlay->device_name) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "strdup failed for \"%s\" overlay\n", device_name);
        goto fail;
    }

    overlay->fd = open(device_name, O_RDWR);

    if (overlay->fd < 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "open failed for \"%s\" overlay: %s\n",
                   device_name, strerror(errno));
        goto fail;
    }

    return overlay;

fail:
    free(overlay->device_name);
    free(overlay);

    return NULL;
}

void vigs_video_overlay_destroy(struct vigs_video_overlay *overlay)
{
    VIGS_LOG_TRACE("device_name = %s", overlay->device_name);

    vigs_video_overlay_stream_off(overlay);

    if (overlay->mmap_ptr) {
        munmap(overlay->mmap_ptr, overlay->mmap_size);
    }

    close(overlay->fd);
    free(overlay->device_name);
    free(overlay);
}

Bool vigs_video_overlay_set_viewport(struct vigs_video_overlay *overlay,
                                     xRectangle *viewport)
{
    struct v4l2_format format;

    VIGS_LOG_TRACE("device_name = %s", overlay->device_name);

    if (!vigs_video_overlay_query_caps(overlay, V4L2_CAP_VIDEO_OVERLAY)) {
        return FALSE;
    }

    memset(&format, 0, sizeof(format));

    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;

    if (!vigs_video_overlay_get_format(overlay, &format)) {
        return FALSE;
    }

    format.fmt.win.w.left = viewport->x;
    format.fmt.win.w.top = viewport->y;
    format.fmt.win.w.width = viewport->width;
    format.fmt.win.w.height = viewport->height;

    if (!vigs_video_overlay_set_format(overlay, &format)) {
        return FALSE;
    }

    if ((memcmp(&overlay->viewport, viewport, sizeof(*viewport)) != 0) &&
        overlay->mmap_ptr) {
        /*
         * Viewport changed, unmap.
         */
        munmap(overlay->mmap_ptr, overlay->mmap_size);
        overlay->mmap_ptr = NULL;
        overlay->mmap_size = 0;
    }

    overlay->viewport = *viewport;

    return TRUE;
}

xRectangle *vigs_video_overlay_viewport(struct vigs_video_overlay *overlay)
{
    return &overlay->viewport;
}

unsigned char *vigs_video_overlay_ptr(struct vigs_video_overlay *overlay)
{
    ScrnInfoPtr scrn = overlay->screen->scrn;
    struct v4l2_buffer buffer;
    void* tmp;

    if (overlay->mmap_ptr) {
        return overlay->mmap_ptr;
    }

    memset(&buffer, 0, sizeof(buffer));

    buffer.index = 0;
    buffer.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (!vigs_video_overlay_query_buf(overlay, &buffer)) {
        return NULL;
    }

    tmp = mmap(NULL, buffer.length,
               PROT_READ | PROT_WRITE,
               MAP_SHARED,
               overlay->fd,
               buffer.m.offset & ~(PAGE_SIZE - 1));

    if (tmp == MAP_FAILED) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "mmap failed for \"%s\": %s\n",
                   overlay->device_name, strerror(errno));
        return NULL;
    }

    overlay->mmap_ptr = tmp;
    overlay->mmap_ptr += buffer.m.offset & (PAGE_SIZE - 1);
    overlay->mmap_size = buffer.length;

    return overlay->mmap_ptr;
}

Bool vigs_video_overlay_stream_on(struct vigs_video_overlay *overlay)
{
    if (overlay->is_stream_on) {
        return TRUE;
    }

    VIGS_LOG_TRACE("device_name = %s", overlay->device_name);

    overlay->is_stream_on = vigs_video_overlay_start(overlay);

    return overlay->is_stream_on;
}

void vigs_video_overlay_stream_off(struct vigs_video_overlay *overlay)
{
    if (overlay->is_stream_on) {
        VIGS_LOG_TRACE("device_name = %s", overlay->device_name);
        vigs_video_overlay_stop(overlay);
        overlay->is_stream_on = FALSE;
    }
}
