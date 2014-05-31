#include "vigs_pixmap.h"
#include "vigs_screen.h"
#include "vigs_drm.h"
#include "vigs_log.h"
#include "vigs.h"

#if HAS_DEVPRIVATEKEYREC
DevPrivateKeyRec vigs_pixmap_index;
#else
int vigs_pixmap_index;
#endif

Bool vigs_pixmap_subsystem_init(struct vigs_screen *vigs_screen)
{
#if HAS_DIXREGISTERPRIVATEKEY
    if (!dixRegisterPrivateKey(&vigs_pixmap_index, PRIVATE_PIXMAP, 0)) {
#else
    if (!dixRequestPrivate(&vigs_pixmap_index, 0)) {
#endif
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to request pixmap private\n");
        return FALSE;
    }

    return TRUE;
}

PixmapPtr vigs_pixmap_create(struct vigs_screen *vigs_screen,
                             uint32_t width,
                             uint32_t height,
                             uint32_t depth,
                             unsigned int usage)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    PixmapPtr pixmap;
    struct vigs_pixmap *vigs_pixmap;
    int ret;

    vigs_pixmap = calloc(sizeof(*vigs_pixmap), 1);

    if (!vigs_pixmap) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate vigs_pixmap\n");
        goto fail1;
    }

    pixmap = fbCreatePixmap(scrn->pScreen, 0, 0, depth, usage);

    if (!pixmap) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate pixmap\n");
        goto fail2;
    }

    vigs_list_init(&vigs_pixmap->list);
    vigs_pixmap->screen = vigs_screen;
    vigs_pixmap->pixmap = pixmap;

    ret = scrn->pScreen->ModifyPixmapHeader(pixmap,
                                            width,
                                            height,
                                            -1,
                                            -1,
                                            (vigs_pixmap_bpp(pixmap) * width),
                                            NULL);

    if (!ret) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to modify pixmap header\n");
        goto fail3;
    }

    vigs_pixmap_set_private(pixmap, vigs_pixmap);

    VIGS_LOG_TRACE("Pixmap %p created %ux%ux%u, depth = %u, usage = %u",
                   pixmap,
                   vigs_pixmap_width(pixmap),
                   vigs_pixmap_height(pixmap),
                   vigs_pixmap_bpp(pixmap),
                   vigs_pixmap_depth(pixmap),
                   usage);

    return pixmap;

fail3:
    fbDestroyPixmap(pixmap);
fail2:
    free(vigs_pixmap);
fail1:

    return NULL;
}

Bool vigs_pixmap_create_from_surface(PixmapPtr pixmap,
                                     struct vigs_drm_surface *sfc)
{
    ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_pixmap *vigs_pixmap;
    int ret;

    vigs_pixmap = calloc(sizeof(*vigs_pixmap), 1);

    if (!vigs_pixmap) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to allocate pixmap\n");
        goto fail1;
    }

    vigs_list_init(&vigs_pixmap->list);
    vigs_pixmap->screen = vigs_screen;
    vigs_pixmap->pixmap = pixmap;
    vigs_pixmap->sfc = sfc;

    ret = vigs_drm_gem_map(&sfc->gem, 1);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to map FB GEM: %s\n", strerror(-ret));
        goto fail2;
    }

    ret = scrn->pScreen->ModifyPixmapHeader(pixmap,
                                            sfc->width,
                                            sfc->height,
                                            -1,
                                            -1,
                                            sfc->stride,
                                            sfc->gem.vaddr);

    if (!ret) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to modify pixmap header\n");
        goto fail2;
    }

    /*
     * Take a reference now cause there won't be any errors ahead.
     */
    vigs_drm_gem_ref(&sfc->gem);

    if (pixmap_to_vigs_pixmap(pixmap)) {
        vigs_pixmap_destroy(pixmap);
    }

    vigs_pixmap_set_private(pixmap, vigs_pixmap);

    VIGS_LOG_TRACE("Pixmap %p created %ux%ux%u, depth = %u",
                   pixmap,
                   vigs_pixmap_width(pixmap),
                   vigs_pixmap_height(pixmap),
                   vigs_pixmap_bpp(pixmap),
                   vigs_pixmap_depth(pixmap));

    return TRUE;

fail2:
    free(vigs_pixmap);
fail1:

    return FALSE;
}

void vigs_pixmap_destroy(PixmapPtr pixmap)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);

    assert(vigs_pixmap);

    if (!vigs_pixmap) {
        return;
    }

    vigs_pixmap_set_private(pixmap, NULL);

    if (vigs_pixmap->sfc) {
        vigs_drm_gem_unref(&vigs_pixmap->sfc->gem);
    }

    vigs_pixmap->screen->scrn->pScreen->ModifyPixmapHeader(pixmap,
                                                           -1,
                                                           -1,
                                                           -1,
                                                           -1,
                                                           -1,
                                                           NULL);

    vigs_list_remove(&vigs_pixmap->list);

    if (vigs_pixmap->vram_dirty_region) {
        REGION_DESTROY(vigs_pixmap->screen->scrn->pScreen,
                       vigs_pixmap->vram_dirty_region);
        vigs_pixmap->vram_dirty_region = NULL;
    }

    free(vigs_pixmap);
}

void vigs_pixmap_set_vram_dirty(struct vigs_pixmap *vigs_pixmap,
                                int x, int y, int w, int h)
{
    RegionRec tmp_reg;
    BoxRec tmp_box;
    int vram_was_dirty = vigs_pixmap_is_vram_dirty(vigs_pixmap);

    tmp_box.x1 = x;
    tmp_box.x2 = x + w;
    tmp_box.y1 = y;
    tmp_box.y2 = y + h;

    REGION_INIT(vigs_pixmap->screen->scrn->pScreen, &tmp_reg, &tmp_box, 1);

    if (vigs_pixmap->vram_dirty_region) {
        REGION_UNION(vigs_pixmap->screen->scrn->pScreen,
                     vigs_pixmap->vram_dirty_region,
                     vigs_pixmap->vram_dirty_region,
                     &tmp_reg);
    } else {
        vigs_pixmap->vram_dirty_region =
            REGION_CREATE(vigs_pixmap->screen->scrn->pScreen,
                          REGION_RECTS(&tmp_reg),
                          REGION_NUM_RECTS(&tmp_reg));
        if (!vigs_pixmap->vram_dirty_region) {
            xf86DrvMsg(vigs_pixmap->screen->scrn->scrnIndex, X_ERROR,
                       "Cannot create region\n");
            goto out;
        }
    }

    if (!vram_was_dirty) {
        assert(vigs_list_empty(&vigs_pixmap->list));
        vigs_list_add_tail(&vigs_pixmap->screen->dirty_vram_pixmaps,
                           &vigs_pixmap->list);
    }

out:
    REGION_UNINIT(vigs_pixmap->screen->scrn->pScreen, &tmp_reg);
}

void vigs_pixmap_set_vram_not_dirty(struct vigs_pixmap *vigs_pixmap)
{
    if (vigs_pixmap_is_vram_dirty(vigs_pixmap)) {
        assert(!vigs_list_empty(&vigs_pixmap->list));
        vigs_list_remove(&vigs_pixmap->list);

        REGION_EMPTY(vigs_pixmap->screen->scrn->pScreen,
                     vigs_pixmap->vram_dirty_region);
    }
}

int vigs_pixmap_is_vram_dirty(struct vigs_pixmap *vigs_pixmap)
{
    return vigs_pixmap->vram_dirty_region &&
           REGION_NOTEMPTY(vigs_pixmap->screen->scrn->pScreen,
                           vigs_pixmap->vram_dirty_region);
}

void vigs_pixmap_set_gpu_dirty(struct vigs_pixmap *vigs_pixmap, int is_dirty)
{
    vigs_pixmap->is_gpu_dirty = is_dirty;
}

int vigs_pixmap_is_gpu_dirty(struct vigs_pixmap *vigs_pixmap)
{
    return vigs_pixmap->is_gpu_dirty;
}

Bool vigs_pixmap_create_sfc(PixmapPtr pixmap)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    struct vigs_screen *vigs_screen = NULL;
    vigsp_surface_format format;
    int ret;

    assert(vigs_pixmap);

    if (!vigs_pixmap) {
        return FALSE;
    } else {
        vigs_screen = vigs_pixmap->screen;
    }

    if (vigs_pixmap->sfc) {
        return TRUE;
    }

    if (vigs_pixmap_bpp(pixmap) != 4) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Only 32 bpp surfaces are supported for now\n");
        return FALSE;
    }

    switch (vigs_pixmap_depth(pixmap)) {
    case 24:
        format = vigsp_surface_bgrx8888;
        break;
    case 32:
        format = vigsp_surface_bgra8888;
        break;
    default:
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Only 24 and 32 depth surfaces are supported for now\n");
        return FALSE;
    }

    ret = vigs_drm_surface_create(vigs_screen->drm->dev,
                                  vigs_pixmap_width(pixmap),
                                  vigs_pixmap_height(pixmap),
                                  vigs_pixmap_stride(pixmap),
                                  format,
                                  0,
                                  &vigs_pixmap->sfc);

    if (ret != 0) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to create DRM surface: %s\n", strerror(-ret));
        return FALSE;
    }

    VIGS_LOG_TRACE("Created surface for pixmap %p: id = %u",
                   pixmap,
                   vigs_pixmap->sfc->id);

    return TRUE;
}

Bool vigs_pixmap_start_access(PixmapPtr pixmap, int is_read, int is_write)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    struct vigs_screen *vigs_screen = vigs_pixmap->screen;
    int ret;
    uint32_t saf = 0;

    assert(vigs_pixmap);
    assert(vigs_pixmap->sfc);

    if (!vigs_pixmap->sfc->gem.vaddr) {
        ret = vigs_drm_gem_map(&vigs_pixmap->sfc->gem, 1);

        if (ret != 0) {
            xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to map GEM: %s\n", strerror(-ret));
            return FALSE;
        }

        ret = vigs_screen->scrn->pScreen->ModifyPixmapHeader(pixmap,
                                                             vigs_pixmap->sfc->width,
                                                             vigs_pixmap->sfc->height,
                                                             -1,
                                                             -1,
                                                             vigs_pixmap->sfc->stride,
                                                             vigs_pixmap->sfc->gem.vaddr);

        if (!ret) {
            xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to modify pixmap header\n");
            vigs_drm_gem_unmap(&vigs_pixmap->sfc->gem);
            return FALSE;
        }
    }

    if (is_read) {
        saf |= VIGS_DRM_SAF_READ;
    }

    if (is_write) {
        saf |= VIGS_DRM_SAF_WRITE;
    }

    ret = vigs_drm_surface_start_access(vigs_pixmap->sfc, saf);

    if (ret != 0) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to start GEM access: %s\n", strerror(-ret));
    }

    return TRUE;
}

void vigs_pixmap_end_access(PixmapPtr pixmap)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    struct vigs_screen *vigs_screen = vigs_pixmap->screen;
    int ret;

    assert(vigs_pixmap);
    assert(vigs_pixmap->sfc);

    ret = vigs_drm_surface_end_access(vigs_pixmap->sfc, 0);

    if (ret != 0) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to end GEM access: %s\n", strerror(-ret));
    }
}

Bool vigs_pixmap_get_name(PixmapPtr pixmap)
{
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    struct vigs_screen *vigs_screen = vigs_pixmap->screen;
    int ret;

    assert(vigs_pixmap);
    assert(vigs_pixmap->sfc);

    ret = vigs_drm_gem_get_name(&vigs_pixmap->sfc->gem);

    if (ret != 0) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to get GEM name: %s\n", strerror(-ret));
        return FALSE;
    }

    return TRUE;
}
