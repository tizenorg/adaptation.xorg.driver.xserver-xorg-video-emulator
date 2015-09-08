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

#include "vigs_utils.h"
#include <time.h>

void vigs_drm_mode_to_mode(ScrnInfoPtr scrn,
                           drmModeModeInfo *drm_mode,
                           DisplayModePtr mode)
{
    memset(mode, 0, sizeof(*mode));

    mode->status = MODE_OK;

    mode->Clock = drm_mode->clock;

    mode->HDisplay = drm_mode->hdisplay;
    mode->HSyncStart = drm_mode->hsync_start;
    mode->HSyncEnd = drm_mode->hsync_end;
    mode->HTotal = drm_mode->htotal;
    mode->HSkew = drm_mode->hskew;

    mode->VDisplay = drm_mode->vdisplay;
    mode->VSyncStart = drm_mode->vsync_start;
    mode->VSyncEnd = drm_mode->vsync_end;
    mode->VTotal = drm_mode->vtotal;
    mode->VScan = drm_mode->vscan;

    mode->Flags = drm_mode->flags;

    mode->name = strdup(drm_mode->name);

    if (drm_mode->type & DRM_MODE_TYPE_DRIVER) {
        mode->type = M_T_DRIVER;
    }

    if (drm_mode->type & DRM_MODE_TYPE_PREFERRED) {
        mode->type |= M_T_PREFERRED;
    }

    xf86SetModeCrtc(mode, scrn->adjustFlags);
}

void vigs_mode_to_drm_mode(ScrnInfoPtr scrn,
                           DisplayModePtr mode,
                           drmModeModeInfo *drm_mode)
{
    memset(drm_mode, 0, sizeof(*drm_mode));

    drm_mode->clock = mode->Clock;
    drm_mode->hdisplay = mode->HDisplay;
    drm_mode->hsync_start = mode->HSyncStart;
    drm_mode->hsync_end = mode->HSyncEnd;
    drm_mode->htotal = mode->HTotal;
    drm_mode->hskew = mode->HSkew;

    drm_mode->vdisplay = mode->VDisplay;
    drm_mode->vsync_start = mode->VSyncStart;
    drm_mode->vsync_end = mode->VSyncEnd;
    drm_mode->vtotal = mode->VTotal;
    drm_mode->vscan = mode->VScan;

    drm_mode->flags = mode->Flags;

    if (mode->name) {
        strncpy(drm_mode->name, mode->name, DRM_DISPLAY_MODE_LEN);
    }

    drm_mode->name[DRM_DISPLAY_MODE_LEN - 1] = 0;
}

PropertyPtr vigs_get_window_property(WindowPtr window,
                                     const char *property_name)
{
    int rc;
    Atom atom;
    PropertyPtr property;

    atom = MakeAtom(property_name, strlen(property_name), FALSE);
    if (atom == None) {
        return NULL;
    }

    rc = dixLookupProperty(&property,
                           window,
                           atom,
                           serverClient,
                           DixReadAccess);

    if ((rc == Success) && property->data) {
        return property;
    }

    return NULL;
}

PixmapPtr vigs_get_drawable_pixmap(DrawablePtr drawable)
{
    if (drawable->type == DRAWABLE_PIXMAP) {
        return (PixmapPtr)drawable;
    } else {
        return (*drawable->pScreen->GetWindowPixmap)((WindowPtr)drawable);
    }
}

void vigs_pixman_convert_image(pixman_op_t op,
                               unsigned char *srcbuf,
                               unsigned char *dstbuf,
                               pixman_format_code_t src_format,
                               pixman_format_code_t dst_format,
                               xRectangle *img_rect,
                               xRectangle *pxm_rect,
                               xRectangle *src_rect,
                               xRectangle *dst_rect,
                               RegionPtr clip_region,
                               int rotation,
                               int is_hflip,
                               int is_vflip)
{
    pixman_image_t *src_img = NULL;
    pixman_image_t *dst_img = NULL;
    struct pixman_f_transform ft;
    pixman_transform_t transform;
    int src_stride, dst_stride;
    int src_bpp;
    int dst_bpp;
    double scale_x, scale_y;
    int rotate_step;

    assert((rotation <= 360) && (rotation >= -360));

    src_bpp = PIXMAN_FORMAT_BPP(src_format) / 8;
    if (src_bpp <= 0) {
        ErrorF("Bad src_format, logic error\n");
        goto out;
    }

    dst_bpp = PIXMAN_FORMAT_BPP(dst_format) / 8;
    if (dst_bpp <= 0) {
        ErrorF("Bad dst_format, logic error\n");
        goto out;
    }

    rotate_step = (rotation + 360) / 90 % 4;

    src_stride = img_rect->width * src_bpp;
    dst_stride = pxm_rect->width * dst_bpp;

    src_img = pixman_image_create_bits(src_format, img_rect->width,
                                       img_rect->height, (uint32_t*)srcbuf,
                                       src_stride);

    if (!src_img) {
        ErrorF("Cannot create src_img\n");
        goto out;
    }

    dst_img = pixman_image_create_bits(dst_format, pxm_rect->width,
                                       pxm_rect->height, (uint32_t*)dstbuf,
                                       dst_stride);

    if (!dst_img) {
        ErrorF("Cannot create dst_img\n");
        goto out;
    }

    pixman_f_transform_init_identity(&ft);

    if (is_hflip) {
        pixman_f_transform_scale(&ft, NULL, -1, 1);
        pixman_f_transform_translate(&ft, NULL, dst_rect->width, 0);
    }

    if (is_vflip) {
        pixman_f_transform_scale(&ft, NULL, 1, -1);
        pixman_f_transform_translate(&ft, NULL, 0, dst_rect->height);
    }

    if (rotate_step > 0) {
        int c, s, tx = 0, ty = 0;
        switch (rotate_step) {
        case 1:
            /* 90 degrees */
            c = 0;
            s = -1;
            tx = -dst_rect->width;
            break;
        case 2:
            /* 180 degrees */
            c = -1;
            s = 0;
            tx = -dst_rect->width;
            ty = -dst_rect->height;
            break;
        case 3:
            /* 270 degrees */
            c = 0;
            s = 1;
            ty = -dst_rect->height;
            break;
        default:
            /* 0 degrees */
            c = 1;
            s = 0;
            ErrorF("Invalid rotate step, step 0 should not happen\n");
            break;
        }

        pixman_f_transform_translate(&ft, NULL, tx, ty);
        pixman_f_transform_rotate(&ft, NULL, c, s);
    }

    if (rotate_step % 2 == 0) {
        scale_x = (double)src_rect->width / dst_rect->width;
        scale_y = (double)src_rect->height / dst_rect->height;
    } else {
        scale_x = (double)src_rect->width / dst_rect->height;
        scale_y = (double)src_rect->height / dst_rect->width;
    }

    pixman_f_transform_scale(&ft, NULL, scale_x, scale_y);
    pixman_f_transform_translate(&ft, NULL, src_rect->x, src_rect->y);

    pixman_transform_from_pixman_f_transform(&transform, &ft);
    pixman_image_set_transform(src_img, &transform);

    pixman_image_composite(op, src_img, NULL, dst_img,
                           0, 0, 0, 0, dst_rect->x, dst_rect->y,
                           dst_rect->width, dst_rect->height);

out:
    if (src_img) {
        pixman_image_unref(src_img);
    }
    if (dst_img) {
        pixman_image_unref(dst_img);
    }
}

uint64_t vigs_gettime_us()
{
    struct timespec tv;

    if (clock_gettime(CLOCK_MONOTONIC, &tv)) {
        return 0;
    }

    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_nsec / 1000;
}

static void *vigs_copy_one_channel(int width, int height,
                                   char *s, int s_size_w, int s_pitches,
                                   char *d, int d_size_w, int d_pitches)
{
    unsigned char *src = (unsigned char*)s;
    unsigned char *dst = (unsigned char*)d;

    if (d_size_w == width && s_size_w == width) {
        memcpy (dst, src, s_pitches * height);
    } else {
        int i;

        for (i = 0; i < height; ++i) {
            memcpy(dst, src, s_pitches);
            src += s_pitches;
            dst += d_pitches;
        }
    }

    return dst;
}

void *vigs_copy_image(int width, int height,
                      char *s, int s_size_w, int s_size_h,
                      int *s_pitches, int *s_offsets, int *s_lengths,
                      char *d, int d_size_w, int d_size_h,
                      int *d_pitches, int *d_offsets, int *d_lengths,
                      int channel, int h_sampling, int v_sampling)
{
    int i;

    for (i = 0; i < channel; ++i) {
        int c_width = width;
        int c_height = height;

        if (i > 0) {
            c_width = c_width / h_sampling;
            c_height = c_height / v_sampling;
        }

        vigs_copy_one_channel(c_width, c_height,
                              s, s_size_w, s_pitches[i],
                              d, d_size_w, d_pitches[i]);

        s = s + s_lengths[i];
        d = d + d_lengths[i];
    }

    return d;
}
