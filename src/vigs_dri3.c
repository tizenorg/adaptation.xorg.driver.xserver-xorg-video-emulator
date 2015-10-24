#include "vigs_dri3.h"
#include "vigs_screen.h"
#include "vigs_drm.h"
#include "vigs_pixmap.h"
#include "vigs_log.h"
#include "vigs.h"

static int vigs_dri3_open(ScreenPtr screen, RRProviderPtr provider, int *out)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    int fd;

    VIGS_LOG_TRACE("enter");

    fd = vigs_drm_get_client_fd(vigs_screen->drm);

    if (fd < 0) {
        VIGS_LOG_ERROR("Failed to get client FD");
        return BadAlloc;
    }

    *out = fd;

    return Success;
}

static PixmapPtr vigs_dri3_pixmap_from_fd(ScreenPtr screen,
                                          int fd,
                                          CARD16 width,
                                          CARD16 height,
                                          CARD16 stride,
                                          CARD8 depth,
                                          CARD8 bpp)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_drm_surface *sfc;
    PixmapPtr pixmap;
    uint32_t gem_size, pixmap_size;
    int ret;

    VIGS_LOG_TRACE("enter");

    if (depth < 24 || depth > 32 || depth % 8) {
        VIGS_LOG_ERROR("Bad depth: %d", depth);
        return NULL;
    }

    pixmap = vigs_pixmap_create(vigs_screen, width, height, depth, 0);

    if (!pixmap) {
        VIGS_LOG_ERROR("Pixmap creation failed");
        return NULL;
    }

    ret = vigs_drm_prime_import_fd(vigs_screen->drm->dev, fd, &sfc);

    if (ret) {
        VIGS_LOG_ERROR("FD import failed");
        goto err_free_pixmap;
    }

    gem_size = sfc->gem.size;
    pixmap_size = height * stride;

    if (pixmap_size > gem_size) {
        VIGS_LOG_ERROR("Pixmap[%d]/Sfc[%d] size mismatch",
                       pixmap_size,
                       gem_size);
        goto err_gem_unref;
    }

    if (vigs_pixmap_create_from_surface(pixmap, sfc)) {
        vigs_drm_gem_unref(&sfc->gem);
        return pixmap;
    }

    VIGS_LOG_ERROR("Sfc attachment failed");

err_gem_unref:
    vigs_drm_gem_unref(&sfc->gem);

err_free_pixmap:
    vigs_pixmap_destroy(pixmap);
    fbDestroyPixmap(pixmap);

    return NULL;
}

static int vigs_dri3_fd_from_pixmap(ScreenPtr screen,
                                    PixmapPtr pixmap,
                                    CARD16 *stride,
                                    CARD32 *size)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    struct vigs_pixmap *vigs_pixmap = pixmap_to_vigs_pixmap(pixmap);
    int fd, ret;

    VIGS_LOG_TRACE("enter");

    if (!vigs_pixmap) {
        VIGS_LOG_ERROR("Pixmap priv is NULL");
        return -1;
    }

    if (!vigs_pixmap_create_sfc(pixmap)) {
        VIGS_LOG_ERROR("Pixmap sfc is NULL");
        return -1;
    }

    if (vigs_pixmap_stride(pixmap) > UINT16_MAX) {
        VIGS_LOG_ERROR("Bad pixmap stride: %d", vigs_pixmap_stride(pixmap));
        return -1;
    }

    ret = vigs_drm_prime_export_fd(vigs_screen->drm->dev,
                                   vigs_pixmap->sfc,
                                   &fd);

    if (ret) {
        VIGS_LOG_ERROR("FD export failed");
        return ret;
    }

    *stride = vigs_pixmap_stride(pixmap);
    *size = vigs_pixmap->sfc->gem.size;

    return fd;
}

static dri3_screen_info_rec vigs_dri3_info = {
    .version = DRI3_SCREEN_INFO_VERSION,

    .open = vigs_dri3_open,
    .pixmap_from_fd = vigs_dri3_pixmap_from_fd,
    .fd_from_pixmap = vigs_dri3_fd_from_pixmap,
};

Bool vigs_dri3_init(struct vigs_screen *vigs_screen)
{
    ScreenPtr screen = vigs_screen->scrn->pScreen;

    VIGS_LOG_TRACE("enter");

    if (!miSyncShmScreenInit(screen))
        return FALSE;

    return dri3_screen_init(screen, &vigs_dri3_info);
}

void vigs_dri3_close(struct vigs_screen *vigs_screen)
{
    VIGS_LOG_TRACE("enter");
}
