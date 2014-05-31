#include "vigs_uxa.h"
#include "vigs_screen.h"
#include "vigs_pixmap.h"
#include "vigs_log.h"
#include "vigs_comm.h"
#include "vigs.h"

static void vigs_uxa_update_gpu(struct vigs_pixmap *vigs_pixmap)
{
    if (vigs_pixmap_is_vram_dirty(vigs_pixmap)) {
        vigs_comm_update_gpu(vigs_pixmap->screen->comm,
                             vigs_pixmap->sfc->id,
                             vigs_pixmap->vram_dirty_region);
        vigs_pixmap_set_vram_not_dirty(vigs_pixmap);
    }
}

/*
 * Solid fill.
 * @{
 */

static Bool vigs_uxa_check_solid(DrawablePtr drawable, int alu, Pixel planemask)
{
    return TRUE;
}

static Bool vigs_uxa_prepare_solid(PixmapPtr pixmap, int alu, Pixel planemask, Pixel fg)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    if (!vigs_pixmap) {
        VIGS_LOG_TRACE("Software solid fill to pixmap %p",
                       pixmap);

        return FALSE;
    }

    if (alu != GXcopy) {
        VIGS_LOG_TRACE("alu = %d, software solid fill to pixmap %p",
                       alu,
                       pixmap);

        return FALSE;
    }

    if (!vigs_pixmap_create_sfc(pixmap)) {
        return FALSE;
    }

    vigs_uxa_update_gpu(vigs_pixmap);
    vigs_pixmap_set_gpu_dirty(vigs_pixmap, 1);

    vigs_comm_solid_fill_prepare(vigs_pixmap->screen->comm,
                                 vigs_pixmap->sfc->id,
                                 fg);

    return TRUE;
}

static void vigs_uxa_solid(PixmapPtr pixmap, int x1, int y1, int x2, int y2)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    assert(vigs_pixmap);

    vigs_comm_solid_fill(vigs_pixmap->screen->comm,
                         x1,
                         y1,
                         x2,
                         y2);
}

static void vigs_uxa_done_solid(PixmapPtr pixmap)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    assert(vigs_pixmap);

    vigs_comm_solid_fill_done(vigs_pixmap->screen->comm);
}

/*
 * @}
 */

/*
 * Copy.
 * @{
 */

static Bool vigs_uxa_check_copy(PixmapPtr source, PixmapPtr dest,
                                int alu, Pixel planemask)
{
    struct vigs_pixmap *vigs_source = pixmap_to_vigs_pixmap(source);
    struct vigs_pixmap *vigs_dest = pixmap_to_vigs_pixmap(dest);

    if (alu != GXcopy) {
        VIGS_LOG_TRACE("alu = %d, software copy to pixmap %p", alu, dest);

        return FALSE;
    }

    if (vigs_source && vigs_dest) {
        return TRUE;
    } else {
        VIGS_LOG_TRACE("Software copy to pixmap %p", dest);

        return FALSE;
    }
}

static Bool vigs_uxa_prepare_copy(PixmapPtr source, PixmapPtr dest, int xdir,
                                  int ydir, int alu, Pixel planemask)
{
    struct vigs_pixmap *vigs_source = pixmap_to_vigs_pixmap(source);
    struct vigs_pixmap *vigs_dest = pixmap_to_vigs_pixmap(dest);

    assert(vigs_source && vigs_dest);

    vigs_pixmap_create_sfc(source);
    vigs_pixmap_create_sfc(dest);

    if (!vigs_source->sfc || !vigs_dest->sfc) {
        return FALSE;
    }

    vigs_uxa_update_gpu(vigs_source);
    vigs_uxa_update_gpu(vigs_dest);
    vigs_pixmap_set_gpu_dirty(vigs_dest, 1);

    vigs_comm_copy_prepare(vigs_source->screen->comm,
                           vigs_source->sfc->id,
                           vigs_dest->sfc->id);

    return TRUE;
}

static void vigs_uxa_copy(PixmapPtr dest, int src_x1, int src_y1, int dst_x1,
                          int dst_y1, int w, int h)
{
    struct vigs_pixmap *vigs_dest = pixmap_to_vigs_pixmap(dest);

    vigs_comm_copy(vigs_dest->screen->comm,
                   src_x1,
                   src_y1,
                   dst_x1,
                   dst_y1,
                   w,
                   h);
}

static void vigs_uxa_done_copy(PixmapPtr pixmap)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    vigs_comm_copy_done(vigs_pixmap->screen->comm);
}

/*
 * @}
 */

/*
 * Composite.
 * @{
 */

static Bool vigs_uxa_check_composite(int op,
                                     PicturePtr source_picture,
                                     PicturePtr mask_picture,
                                     PicturePtr dest_picture,
                                     int width, int height)
{
    VIGS_LOG_TRACE("Software compositing to pixmap %p",
                   dest_picture->pDrawable);
    return FALSE;
}

static Bool vigs_uxa_check_composite_target(PixmapPtr pixmap)
{
    return FALSE;
}

static Bool vigs_uxa_check_composite_texture(ScreenPtr screen,
                                             PicturePtr picture)
{
    return FALSE;
}

static Bool vigs_uxa_prepare_composite(int op,
                                       PicturePtr source_picture,
                                       PicturePtr mask_picture,
                                       PicturePtr dest_picture,
                                       PixmapPtr source,
                                       PixmapPtr mask,
                                       PixmapPtr dest)
{
    VIGS_LOG_TRACE("Software compositing to pixmap %p", dest);
    return FALSE;
}

static void vigs_uxa_composite(PixmapPtr dest,
                               int src_x,
                               int src_y,
                               int mask_x,
                               int mask_y,
                               int dst_x,
                               int dst_y,
                               int w,
                               int h)
{
}

static void vigs_uxa_done_composite(PixmapPtr dest)
{
}

/*
 * @}
 */

/*
 * PutImage.
 * @{
 */

static Bool vigs_uxa_put_image(PixmapPtr pixmap,
                               int x, int y,
                               int w, int h,
                               char *src, int src_pitch)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    int stride = vigs_pixmap_stride(pixmap);
    int bpp = vigs_pixmap_bpp(pixmap);
    int row_length = w * bpp;
    char *dst;
    int num_rows;

    if (!vigs_pixmap) {
        VIGS_LOG_TRACE("Software put image to pixmap %p", pixmap);
        return FALSE;
    }

    if (row_length != src_pitch) {
        VIGS_LOG_ERROR("Custom strides not supported yet, software put image to pixmap %p", pixmap);
        return FALSE;
    }

    if (!vigs_uxa_raw_access(vigs_pixmap,
                             x, y,
                             w, h,
                             vigs_uxa_access_write)) {
        return TRUE;
    }

    dst = pixmap->devPrivate.ptr;
    num_rows = h;

    if (src_pitch == stride) {
        num_rows = 1;
        row_length *= h;
    }

    dst += (y * stride) + (x * bpp);
    do {
        memcpy(dst, src, row_length);
        src += src_pitch;
        dst += stride;
    } while (--num_rows);

    vigs_uxa_end_raw_access(vigs_pixmap);

    return TRUE;
}

static Bool vigs_uxa_get_image(PixmapPtr pixmap,
                               int x, int y, int w, int h,
                               char *dst, int dst_pitch)
{
    VIGS_LOG_TRACE("Software get image from pixmap %p %d,%d %dx%d",
                   pixmap, x, y, w, h);
    return FALSE;
}

/*
 * @}
 */

/*
 * Pixmap access.
 * @{
 */

static Bool vigs_uxa_prepare_access(PixmapPtr pixmap, uxa_access_t access)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    vigs_uxa_access vigs_access;

    if (!vigs_pixmap) {
        return TRUE;
    }

    if ((access == UXA_ACCESS_RW) ||
        (access == UXA_GLAMOR_ACCESS_RW)) {
        vigs_access = vigs_uxa_access_readwrite;
    } else {
        vigs_access = vigs_uxa_access_read;
    }

    return vigs_uxa_raw_access(vigs_pixmap,
                               0, 0,
                               vigs_pixmap_width(pixmap),
                               vigs_pixmap_height(pixmap),
                               vigs_access);
}

static void vigs_uxa_finish_access(PixmapPtr pixmap, uxa_access_t access)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    if (!vigs_pixmap) {
        return;
    }

    vigs_uxa_end_raw_access(vigs_pixmap);
}

static Bool vigs_uxa_pixmap_is_offscreen(PixmapPtr pixmap)
{
    return (pixmap_to_vigs_pixmap(pixmap) ? TRUE : FALSE);
}

/*
 * @}
 */

static PixmapPtr vigs_uxa_create_pixmap(ScreenPtr screen,
                                        int w, int h, int depth,
                                        unsigned usage)
{
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    PixmapPtr pixmap;

    /*
     * non-24 and non-32 depth pixmaps are software.
     * 0x0 pixmaps are software.
     */
    if (((depth != 24) && (depth != 32)) ||
        (w <= 0) || (h <= 0)) {
        pixmap = fbCreatePixmap(screen, w, h, depth, usage);

        VIGS_LOG_TRACE("Software pixmap %p created %ux%ux%u, depth = %u",
                       pixmap,
                       vigs_pixmap_width(pixmap),
                       vigs_pixmap_height(pixmap),
                       vigs_pixmap_bpp(pixmap),
                       vigs_pixmap_depth(pixmap));

        return pixmap;
    }

    /*
     * We don't handle small glyphs.
     */
    if ((usage == CREATE_PIXMAP_USAGE_GLYPH_PICTURE) &&
        (w <= 32) &&
        (h <= 32)) {
        pixmap = fbCreatePixmap(screen, w, h, depth, usage);

        VIGS_LOG_TRACE("Software pixmap %p created %ux%ux%u, depth = %u",
                       pixmap,
                       vigs_pixmap_width(pixmap),
                       vigs_pixmap_height(pixmap),
                       vigs_pixmap_bpp(pixmap),
                       vigs_pixmap_depth(pixmap));

        return pixmap;
    }

    pixmap = vigs_pixmap_create(vigs_screen, w, h,
                                depth, usage);

    return pixmap;
}

static Bool vigs_uxa_destroy_pixmap(PixmapPtr pixmap)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    if ((pixmap->refcnt == 1)) {
        if (vigs_pixmap) {
            vigsp_surface_id id = 0;

            /*
             * Flush here because pixmap being deleted might have taken part
             * in batched operations.
             */
            vigs_comm_flush(vigs_pixmap->screen->comm);

            if (vigs_pixmap->sfc) {
                id = vigs_pixmap->sfc->id;
            }

            vigs_pixmap_destroy(pixmap);

            VIGS_LOG_TRACE("Pixmap %p(sfc = %u) destroyed",
                           pixmap,
                           id);
        } else {
            VIGS_LOG_TRACE("Software pixmap %p destroyed",
                           pixmap);
        }
    }

    return fbDestroyPixmap(pixmap);
}

Bool vigs_uxa_init(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;

    VIGS_LOG_TRACE("enter");

    assert(!vigs_screen->uxa_driver);

    if (vigs_screen->uxa_driver) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "UXA driver already initialized, logic error\n");
        return FALSE;
    }

    if (!vigs_pixmap_subsystem_init(vigs_screen)) {
        return FALSE;
    }

    vigs_screen->uxa_driver = uxa_driver_alloc();

    if (!vigs_screen->uxa_driver) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate UXA driver structure\n");
        return FALSE;
    }

    vigs_screen->uxa_driver->uxa_major = 1;
    vigs_screen->uxa_driver->uxa_minor = 0;

    /*
     * Solid fill.
     * @{
     */

    vigs_screen->uxa_driver->check_solid = vigs_uxa_check_solid;
    vigs_screen->uxa_driver->prepare_solid = vigs_uxa_prepare_solid;
    vigs_screen->uxa_driver->solid = vigs_uxa_solid;
    vigs_screen->uxa_driver->done_solid = vigs_uxa_done_solid;

    /*
     * @}
     */

    /*
     * Copy.
     * @{
     */

    vigs_screen->uxa_driver->check_copy = vigs_uxa_check_copy;
    vigs_screen->uxa_driver->prepare_copy = vigs_uxa_prepare_copy;
    vigs_screen->uxa_driver->copy = vigs_uxa_copy;
    vigs_screen->uxa_driver->done_copy = vigs_uxa_done_copy;

    /*
     * @}
     */

    /*
     * Composite.
     * @{
     */

    vigs_screen->uxa_driver->check_composite = vigs_uxa_check_composite;
    vigs_screen->uxa_driver->check_composite_target = vigs_uxa_check_composite_target;
    vigs_screen->uxa_driver->check_composite_texture = vigs_uxa_check_composite_texture;
    vigs_screen->uxa_driver->prepare_composite = vigs_uxa_prepare_composite;
    vigs_screen->uxa_driver->composite = vigs_uxa_composite;
    vigs_screen->uxa_driver->done_composite = vigs_uxa_done_composite;

    /*
     * @}
     */

    /*
     * PutImage.
     * @{
     */

    vigs_screen->uxa_driver->put_image = vigs_uxa_put_image;
    vigs_screen->uxa_driver->get_image = vigs_uxa_get_image;

    /*
     * @}
     */

    /*
     * Pixmap access.
     * @{
     */

    vigs_screen->uxa_driver->prepare_access = vigs_uxa_prepare_access;
    vigs_screen->uxa_driver->finish_access = vigs_uxa_finish_access;
    vigs_screen->uxa_driver->pixmap_is_offscreen = vigs_uxa_pixmap_is_offscreen;

    /*
     * @}
     */

    scrn->pScreen->CreatePixmap = vigs_uxa_create_pixmap;
    scrn->pScreen->DestroyPixmap = vigs_uxa_destroy_pixmap;

    if (!uxa_driver_init(scrn->pScreen, vigs_screen->uxa_driver)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to initialize UXA driver\n");
        return FALSE;
    }

    return TRUE;
}

Bool vigs_uxa_create_screen_resources(struct vigs_screen *vigs_screen)
{
    if (!uxa_resources_init(vigs_screen->scrn->pScreen)) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to initialize UXA screen resources\n");
        return FALSE;
    }

    return TRUE;
}

void vigs_uxa_close(struct vigs_screen *vigs_screen)
{
    VIGS_LOG_TRACE("enter");

    uxa_driver_fini(vigs_screen->scrn->pScreen);

    free(vigs_screen->uxa_driver);
    vigs_screen->uxa_driver = NULL;
}

void vigs_uxa_flush(struct vigs_screen *vigs_screen)
{
    struct vigs_pixmap *vigs_pixmap, *next;

    /*
     * Walk dirty VRAM pixmaps and upload them to GPU.
     */

    vigs_list_for_each_safe(struct vigs_pixmap,
                            vigs_pixmap,
                            next,
                            &vigs_screen->dirty_vram_pixmaps,
                            list) {
        vigs_uxa_update_gpu(vigs_pixmap);
    }

    vigs_comm_flush(vigs_screen->comm);
}

Bool vigs_uxa_raw_access(struct vigs_pixmap *vigs_pixmap,
                         int x, int y,
                         int w, int h,
                         vigs_uxa_access access)
{
    PixmapPtr pixmap = vigs_pixmap->pixmap;

    if (!vigs_pixmap_create_sfc(pixmap)) {
        return FALSE;
    }

    if (vigs_pixmap_is_vram_dirty(vigs_pixmap)) {
        if ((access & vigs_uxa_access_write) != 0) {
            vigs_pixmap_set_vram_dirty(vigs_pixmap,
                                       x, y,
                                       w, h);
        }
    } else {
        int do_flush = 0;

        if (vigs_pixmap_is_gpu_dirty(vigs_pixmap)) {
            vigs_pixmap_set_gpu_dirty(vigs_pixmap, 0);
            do_flush = 1;
        }

        if ((access & vigs_uxa_access_write) != 0) {
            vigs_pixmap_set_vram_dirty(vigs_pixmap,
                                       x, y,
                                       w, h);
            do_flush = 1;
        }

        if (do_flush) {
            vigs_comm_flush(vigs_pixmap->screen->comm);
        }
    }

    return vigs_pixmap_start_access(pixmap,
                                    (access & vigs_uxa_access_read),
                                    (access & vigs_uxa_access_write));
}

void vigs_uxa_end_raw_access(struct vigs_pixmap *vigs_pixmap)
{
    /*
     * It's a performance optimization not to call
     * unmap here, although we should watch out for cases where we might fill
     * the virtual memory space of the process.
     */

    vigs_pixmap_end_access(vigs_pixmap->pixmap);
}
