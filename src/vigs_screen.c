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

#include "vigs_screen.h"
#include "vigs_log.h"
#include "vigs_drm.h"
#include "vigs_uxa.h"
#include "vigs_dri2.h"
#include "vigs_dri3.h"
#include "vigs_present.h"
#include "vigs_xv.h"
#include "vigs_drm_crtc.h"
#include "vigs_cursor.h"
#include "vigs_pixmap.h"
#include "vigs_comm.h"
#include "vigs.h"
#include "micmap.h"
#include "fb.h"
#include "xf86Crtc.h"
#include <errno.h>

#define VIGS_UNIMPLEMENTED(func, scrn) \
    FatalError("VIGS(%d): " #func " not implemented\n", (scrn)->scrnIndex)

static struct vigs_drm_surface
    *vigs_screen_surface_open(struct vigs_screen *vigs_screen,
                              uint32_t fb_id)
{
    drmModeFBPtr drm_fb;
    struct drm_gem_flink flink;
    int ret;
    struct vigs_drm_surface *sfc = NULL;

    drm_fb = drmModeGetFB(vigs_screen->drm->fd, fb_id);

    if (!drm_fb) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to open FB %u\n", fb_id);
        return NULL;
    }

    memset(&flink, 0, sizeof(flink));
    flink.handle = drm_fb->handle;

    ret = drmIoctl(vigs_screen->drm->fd, DRM_IOCTL_GEM_FLINK, &flink);

    if (ret != 0) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to FLINK FB %u: %s\n", fb_id, strerror(-errno));
        goto out;
    }

    ret = vigs_drm_surface_open(vigs_screen->drm->dev, flink.name, &sfc);

    if (ret != 0) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to open FB %u surface: %s\n", fb_id, strerror(-ret));
        goto out;
    }

    VIGS_LOG_TRACE("fb_id = %u, sfc_id = %u", fb_id, sfc->id);

out:
    drmModeFreeFB(drm_fb);

    return sfc;
}

static void vigs_screen_copy_fb(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(scrn);
    int i, ret;
    struct vigs_drm_surface *fb_sfc = NULL;
    uint8_t *buff = NULL;

    for (i = 0; i < crtc_config->num_crtc; ++i) {
        struct vigs_drm_crtc *crtc = crtc_config->crtc[i]->driver_private;
        if (crtc->mode_crtc->buffer_id) {
            fb_sfc = vigs_screen_surface_open(vigs_screen,
                                              crtc->mode_crtc->buffer_id);

            /*
             * We opened fbcon DRM surface, if its format matches
             * that of 'front_sfc', then copy it to 'front_sfc'.
             */

            if (fb_sfc &&
                (fb_sfc->stride == vigs_screen->front_sfc->stride) &&
                (fb_sfc->height == vigs_screen->front_sfc->height)) {
                ret = vigs_drm_gem_map(&fb_sfc->gem, 0);

                if (ret != 0) {
                    xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                               "Unable to map fbcon GEM: %s\n",
                               strerror(-ret));
                    break;
                }

                buff = malloc(fb_sfc->stride * fb_sfc->height);

                if (!buff) {
                    xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                               "Unable to allocate %d bytes\n",
                               fb_sfc->stride * fb_sfc->height);
                    break;
                }

                memcpy(buff, fb_sfc->gem.vaddr, fb_sfc->stride * fb_sfc->height);
            }

            break;
        }
    }

    if (fb_sfc) {
        vigs_drm_gem_unref(&fb_sfc->gem);
    }

    if (!buff) {
        /*
         * No fbcon DRM surface or format mismatch, just fill
         * 'front_sfc' with 0.
         */

        buff = malloc(vigs_screen->front_sfc->stride * vigs_screen->front_sfc->height);

        if (buff) {
            memset(buff, 0, vigs_screen->front_sfc->stride * vigs_screen->front_sfc->height);
        } else {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                       "Unable to allocate %d bytes\n",
                       vigs_screen->front_sfc->stride * vigs_screen->front_sfc->height);
            return;
        }
    }

    ret = vigs_drm_gem_map(&vigs_screen->front_sfc->gem, 1);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "Unable to map FB GEM: %s\n",
                   strerror(-ret));
        goto out1;
    }

    ret = vigs_drm_surface_start_access(vigs_screen->front_sfc,
                                        VIGS_DRM_SAF_WRITE);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "Unable to start FB GEM access: %s\n", strerror(-ret));
        goto out2;
    }

    memcpy(vigs_screen->front_sfc->gem.vaddr,
           buff,
           vigs_screen->front_sfc->stride * vigs_screen->front_sfc->height);

    ret = vigs_drm_surface_end_access(vigs_screen->front_sfc, 1);

    if (ret != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "Unable to end FB GEM access: %s\n", strerror(-ret));
    }

out2:
    vigs_drm_gem_unmap(&vigs_screen->front_sfc->gem);
out1:
    free(buff);
}

xf86CrtcPtr vigs_screen_covering_crtc(ScrnInfoPtr scrn,
                                      BoxPtr box,
                                      xf86CrtcPtr desired,
                                      BoxPtr crtc_box_ret)
{
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(scrn);
    xf86CrtcPtr crtc, best_crtc;
    int coverage, best_coverage;
    int c;
    BoxRec crtc_box, cover_box;

    best_crtc = NULL;
    best_coverage = 0;
    crtc_box_ret->x1 = 0;
    crtc_box_ret->x2 = 0;
    crtc_box_ret->y1 = 0;
    crtc_box_ret->y2 = 0;

    for (c = 0; c < crtc_config->num_crtc; c++) {
        crtc = crtc_config->crtc[c];

        /* If the CRTC is off, treat it as not covering */
        if (!vigs_drm_crtc_is_on(crtc)) {
            continue;
        }

        vigs_crtc_box(crtc, &crtc_box);
        vigs_box_intersect(&cover_box, &crtc_box, box);
        coverage = vigs_box_area(&cover_box);

        if (coverage && crtc == desired) {
            *crtc_box_ret = crtc_box;
            return crtc;
        }

        if (coverage > best_coverage) {
            *crtc_box_ret = crtc_box;
            best_crtc = crtc;
            best_coverage = coverage;
        }
    }

    return best_crtc;
}

static Bool vigs_screen_pre_init_visual(ScrnInfoPtr scrn)
{
    Gamma gzeros = { 0.0, 0.0, 0.0 };
    rgb rzeros = { 0, 0, 0 };

    VIGS_LOG_TRACE("enter");

    /*
     * We only support 32bpp framebuffer.
     */
    if (!xf86SetDepthBpp(scrn, 0, 0, 0, Support32bppFb)) {
        return FALSE;
    }

    /*
     * We only support 24bpp depth.
     */
    if ((scrn->depth != 24))
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Depth %d is not supported\n",
                   scrn->depth);
        return FALSE;
    }

    xf86PrintDepthBpp(scrn);

    if (!xf86SetWeight(scrn, rzeros, rzeros)) {
        return FALSE;
    }

    if (!xf86SetDefaultVisual(scrn, -1)) {
        return FALSE;
    }

    if (!xf86SetGamma(scrn, gzeros)) {
        return FALSE;
    }

    return TRUE;
}

static Bool vigs_screen_create_resources(ScreenPtr screen)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    int ret;

    VIGS_LOG_TRACE("scrn_index = %d", screen->myNum);

    screen->CreateScreenResources = vigs_screen->create_screen_resources_fn;
    ret = screen->CreateScreenResources(screen);
    screen->CreateScreenResources = vigs_screen_create_resources;

    if (!ret) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "CreateScreenResources failed\n");
        return FALSE;
    }

    if (vigs_screen->no_accel) {
        /*
         * When no acceleration is available just use front surface's
         * GEM as a root surface.
         */
        ret = vigs_drm_gem_map(&vigs_screen->front_sfc->gem, 0);

        if (ret != 0) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to map FB GEM: %s\n", strerror(-ret));
            return FALSE;
        }

        ret = scrn->pScreen->ModifyPixmapHeader(scrn->pScreen->GetScreenPixmap(scrn->pScreen),
                                                vigs_screen->front_sfc->width,
                                                vigs_screen->front_sfc->height,
                                                -1,
                                                -1,
                                                vigs_screen->front_sfc->stride,
                                                vigs_screen->front_sfc->gem.vaddr);

        if (!ret) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to modify pixmap header\n");
            return FALSE;
        }

        return TRUE;
    } else {
        if (!vigs_uxa_create_screen_resources(vigs_screen)) {
            return FALSE;
        }

        return vigs_pixmap_create_from_surface(
                   scrn->pScreen->GetScreenPixmap(scrn->pScreen),
                   vigs_screen->front_sfc);
    }
}

static void vigs_flush_handler(CallbackListPtr *callback_list,
                               pointer data,
                               pointer call_data)
{
    ScrnInfoPtr scrn = data;
    struct vigs_screen *vigs_screen = scrn->driverPrivate;

    if (scrn->vtSema && !vigs_screen->no_accel) {
        vigs_uxa_flush(vigs_screen);
    }
}

static Bool vigs_screen_close(CLOSE_SCREEN_ARGS_DECL)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    Bool ret;

    VIGS_LOG_TRACE("scrn_index = %d", scrn->scrnIndex);

    if (!vigs_screen->initialized) {
        return TRUE;
    }

    DeleteCallback(&FlushCallback, vigs_flush_handler, scrn);

    vigs_xv_close(vigs_screen);

    if (!vigs_screen->no_accel) {
        vigs_uxa_close(vigs_screen);

        if (vigs_screen->dri3) {
            vigs_present_close(vigs_screen);
            vigs_dri3_close(vigs_screen);
        }

        vigs_dri2_close(vigs_screen);
    }

    vigs_drm_close(vigs_screen);

    vigs_drm_gem_unref(&vigs_screen->front_sfc->gem);
    vigs_screen->front_sfc = NULL;

    if (scrn->vtSema) {
        vigs_screen_leave_vt(VT_FUNC_ARGS(0));
        scrn->vtSema = FALSE;
    }

    vigs_cursor_close(vigs_screen);

    screen->CloseScreen = vigs_screen->close_screen_fn;
    screen->CreateScreenResources = vigs_screen->create_screen_resources_fn;
    screen->BlockHandler = vigs_screen->block_handler_fn;

    ret = (*screen->CloseScreen)(CLOSE_SCREEN_ARGS);

    vigs_screen->initialized = 0;

    return ret;
}

static void vigs_block_handler(BLOCKHANDLER_ARGS_DECL)
{
    SCREEN_PTR(arg);
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;

    screen->BlockHandler = vigs_screen->block_handler_fn;

    (*screen->BlockHandler)(BLOCKHANDLER_ARGS);

    vigs_screen->block_handler_fn = screen->BlockHandler;
    screen->BlockHandler = &vigs_block_handler;

    if (!vigs_screen->no_accel) {
        vigs_uxa_flush(vigs_screen);
    }
}

Bool vigs_screen_pre_init(ScrnInfoPtr scrn, int flags)
{
#if XSERVER_LIBPCIACCESS
    struct pci_device *pci_dev;
#else
    pciVideoPtr pci_dev;
    PCITAG pci_tag;
#endif
    char bus_id[64];
    struct vigs_screen *vigs_screen;
    int tmp;

    VIGS_LOG_TRACE("scrnIndex = %d, flags = 0x%X", scrn->scrnIndex, flags);

    if (flags & PROBE_DETECT) {
        return FALSE;
    }

    if (scrn->driverPrivate) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "VIGS already pre-initialized\n");
        return FALSE;
    }

    vigs_screen = scrn->driverPrivate = xnfcalloc(sizeof(*vigs_screen), 1);

    vigs_list_init(&vigs_screen->dirty_vram_pixmaps);

    vigs_screen->scrn = scrn;
    vigs_screen->ent = xf86GetEntityInfo(scrn->entityList[0]);

    if (xf86IsEntityShared(scrn->entityList[0])) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "No dual-head\n");
        return FALSE;
    }

    if (vigs_screen->ent->location.type != BUS_PCI) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Only primary PCI-devices are supported\n");
        return FALSE;
    }

    pci_dev = xf86GetPciInfoForEntity(vigs_screen->ent->index);

    if (!pci_dev) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Only primary PCI-devices are supported\n");
        return FALSE;
    }

    scrn->monitor = scrn->confScreen->monitor;

    if (!vigs_screen_pre_init_visual(scrn)) {
        return FALSE;
    }

    xf86CollectOptions(scrn, NULL);
    memcpy(vigs_screen->options, g_vigs_options, sizeof(g_vigs_options));
    xf86ProcessOptions(scrn->scrnIndex, scrn->options, vigs_screen->options);

    if (xf86GetOptValInteger(vigs_screen->options,
                             vigs_option_max_execbuffer_size,
                             &tmp) && (tmp >= 4096)) {
        vigs_screen->max_execbuffer_size = tmp;
    } else {
        vigs_screen->max_execbuffer_size =
            g_vigs_options[vigs_option_max_execbuffer_size].value.num;
    }

    vigs_screen->no_accel = xf86ReturnOptValBool(vigs_screen->options,
                                                 vigs_option_no_accel,
                                                 g_vigs_options[vigs_option_no_accel].value.num);
    vigs_screen->hwcursor = xf86ReturnOptValBool(vigs_screen->options,
                                                 vigs_option_hwcursor,
                                                 g_vigs_options[vigs_option_hwcursor].value.num);

    vigs_screen->dri3 = xf86ReturnOptValBool(vigs_screen->options,
                                             vigs_option_dri3,
                                             g_vigs_options[vigs_option_dri3].value.num);

    xf86DrvMsg(scrn->scrnIndex, X_INFO, "Max Execbuffer Size: %u\n",
               vigs_screen->max_execbuffer_size);
    xf86DrvMsg(scrn->scrnIndex, X_INFO, "No accel: %s\n",
               (vigs_screen->no_accel ? "Yes" : "No"));
    xf86DrvMsg(scrn->scrnIndex, X_INFO, "HW Cursor: %s\n",
               (vigs_screen->hwcursor ? "Yes" : "No"));
    xf86DrvMsg(scrn->scrnIndex, X_INFO, "DRI3: %s\n",
               (vigs_screen->dri3 ? "Yes" : "No"));

    scrn->progClock = TRUE;

#if XSERVER_LIBPCIACCESS
    sprintf(bus_id, "PCI:%d:%d:%d", ((pci_dev->domain << 8) | pci_dev->bus),
                                    pci_dev->dev, pci_dev->func);
#else
    sprintf(bus_id, "PCI:%d:%d:%d", ((pciConfigPtr)pci_dev->thisCard)->busnum,
                                    ((pciConfigPtr)pci_dev->thisCard)->devnum,
                                    ((pciConfigPtr)pci_dev->thisCard)->funcnum);
#endif

    if (!vigs_drm_pre_init(vigs_screen, bus_id)) {
        return FALSE;
    }

    if (!vigs_comm_create(vigs_screen,
                          vigs_screen->max_execbuffer_size,
                          &vigs_screen->comm)) {
        return FALSE;
    }

    xf86PruneDriverModes(scrn);

    if (!scrn->modes) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "No usable modes found\n");
        return FALSE;
    }

    scrn->currentMode = scrn->modes;

    xf86PrintModes(scrn);

    xf86SetDpi(scrn, 0, 0);

    if (!xf86LoadSubModule(scrn, "fb")) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to load \"fb\" submodule\n");
        return FALSE;
    }

    if (!vigs_screen->no_accel &&
        !xf86LoadSubModule(scrn, "dri2")) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to load \"dri2\" submodule\n");
        return FALSE;
    }

    if (!vigs_screen->no_accel && vigs_screen->dri3 &&
        !xf86LoadSubModule(scrn, "dri3")) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to load \"dri3\" submodule\n");
        return FALSE;
    }

    xf86DrvMsg(scrn->scrnIndex, X_INFO, "VIGS pre-initialized\n");

    vigs_screen->pre_initialized = 1;

    return TRUE;
}

Bool vigs_screen_init(SCREEN_INIT_ARGS_DECL)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;
    VisualPtr visual;
    int ret = FALSE;
    vigsp_surface_format format;
    uint32_t bpp;

    VIGS_LOG_TRACE("scrn_index = %d, argc = %d", scrn->scrnIndex, argc);

    if (!vigs_list_empty(&vigs_screen->dirty_vram_pixmaps)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Dirty VRAM pixmap list is not empty, logic error\n");
        return FALSE;
    }

    scrn->pScreen = screen;
    vigs_screen->scrn = scrn;

    if (!vigs_drm_init(vigs_screen)) {
        return FALSE;
    }

    if (!vigs_screen->no_accel &&
        !vigs_dri2_init(vigs_screen)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "DRI2 init failed\n");
        return FALSE;
    }

    assert(!vigs_screen->front_sfc);

    if (vigs_screen->front_sfc) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Front surface already exist, logic error\n");
        return FALSE;
    }

    bpp = (scrn->bitsPerPixel + 7) / 8;

    if (bpp != 4) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Only 32 bpp surfaces are supported for now\n");
        return FALSE;
    }

    switch (scrn->depth) {
    case 24:
        format = vigsp_surface_bgrx8888;
        break;
    case 32:
        format = vigsp_surface_bgra8888;
        break;
    default:
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Only 24 and 32 depth surfaces are supported for now\n");
        return FALSE;
    }

    VIGS_LOG_TRACE("Allocating front surface %ux%ux%u, depth = %u",
                   (uint32_t)scrn->virtualX,
                   (uint32_t)scrn->virtualY,
                   bpp,
                   (uint32_t)scrn->depth);

    ret = vigs_drm_surface_create(vigs_screen->drm->dev,
                                  scrn->virtualX,
                                  scrn->virtualY,
                                  ((uint32_t)scrn->virtualX * bpp),
                                  format,
                                  vigs_screen->no_accel,
                                  &vigs_screen->front_sfc);

    if (ret != 0) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to create FB surface: %s\n", strerror(-ret));
        return FALSE;
    }

    scrn->displayWidth = scrn->virtualX;

    vigs_screen_copy_fb(vigs_screen);

    /*
     * Reset visuals.
     */
    miClearVisualTypes();

    /*
     * Setup the visuals we support.
     */
    if (!miSetVisualTypes(scrn->depth,
                          miGetDefaultVisualMask(scrn->depth),
                          scrn->rgbBits,
                          scrn->defaultVisual)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to setup visuals\n");
        return FALSE;
    }

    if (!miSetPixmapDepths()) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to setup pixmap depths\n");
        return FALSE;
    }

    scrn->memPhysBase = 0;
    scrn->fbOffset = 0;

    if (!fbScreenInit(screen, NULL, scrn->virtualX,
                      scrn->virtualY, scrn->xDpi,
                      scrn->yDpi, scrn->displayWidth,
                      scrn->bitsPerPixel)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to setup FB screen\n");
        return FALSE;
    }

    visual = screen->visuals + screen->numVisuals;
    while (--visual >= screen->visuals) {
        if ((visual->class | DynamicClass) == DirectColor) {
            visual->offsetRed = scrn->offset.red;
            visual->offsetGreen = scrn->offset.green;
            visual->offsetBlue = scrn->offset.blue;
            visual->redMask = scrn->mask.red;
            visual->greenMask = scrn->mask.green;
            visual->blueMask = scrn->mask.blue;
        }
    }

    if (!fbPictureInit(screen, NULL, 0)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to setup FB picture\n");
        return FALSE;
    }

    xf86SetBlackWhitePixels(screen);

    if (!vigs_screen->no_accel &&
        !vigs_uxa_init(vigs_screen)) {
        return FALSE;
    }

    if (!vigs_screen->no_accel && vigs_screen->dri3 &&
        !vigs_dri3_init(vigs_screen)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "DRI3 init failed\n");
        return FALSE;
    }

    if (!vigs_screen->no_accel && vigs_screen->dri3 &&
        !vigs_present_init(vigs_screen)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Present init failed\n");
        return FALSE;
    }

    xf86SetBackingStore(screen);
    xf86SetSilkenMouse(screen);

    miDCInitialize(screen, xf86GetPointerScreenFuncs());

    if (vigs_screen->hwcursor &&
        !vigs_cursor_init(vigs_screen)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "HW Cursor init failed\n");
    }

    scrn->vtSema = TRUE;

    screen->SaveScreen = xf86SaveScreen;

    vigs_screen->create_screen_resources_fn = screen->CreateScreenResources;
    screen->CreateScreenResources = vigs_screen_create_resources;

    vigs_screen->close_screen_fn = screen->CloseScreen;
    screen->CloseScreen = vigs_screen_close;

    vigs_screen->block_handler_fn = screen->BlockHandler;
    screen->BlockHandler = vigs_block_handler;

    AddCallback(&FlushCallback, vigs_flush_handler, scrn);

    if (!xf86CrtcScreenInit(screen)) {
        return FALSE;
    }

    if (!miCreateDefColormap(screen)) {
        return FALSE;
    }

    if (!xf86DPMSInit(screen, xf86DPMSSet, 0)) {
        return FALSE;
    }

    if (serverGeneration == 1) {
        xf86ShowUnusedOptions(scrn->scrnIndex, scrn->options);
    }

    if (!vigs_xv_init(vigs_screen)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Xv init failed\n");
        return FALSE;
    }

    xf86DrvMsg(scrn->scrnIndex, X_INFO, "VIGS initialized\n");

    ret = vigs_screen_enter_vt(VT_FUNC_ARGS(0));

    if (ret) {
        vigs_screen->initialized = 1;
    }

    return ret;
}

Bool vigs_screen_switch_mode(SWITCH_MODE_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);

    VIGS_UNIMPLEMENTED(vigs_screen_switch_mode, scrn);

    return FALSE;
}

void vigs_screen_adjust_frame(ADJUST_FRAME_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);

    VIGS_UNIMPLEMENTED(vigs_screen_adjust_frame, scrn);
}

ModeStatus vigs_screen_valid_mode(SCRN_ARG_TYPE arg, DisplayModePtr mode, Bool verbose, int flags)
{
    SCRN_INFO_PTR(arg);

    VIGS_UNIMPLEMENTED(vigs_screen_valid_mode, scrn);

    return TRUE;
}

Bool vigs_screen_enter_vt(VT_FUNC_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;

    VIGS_LOG_TRACE("scrn_index = %d", scrn->scrnIndex);

    scrn->vtSema = TRUE;

    if (!vigs_drm_set_master(vigs_screen->drm)) {
        return FALSE;
    }

    if (!xf86SetDesiredModes(scrn)) {
        return FALSE;
    }

    return TRUE;
}

void vigs_screen_leave_vt(VT_FUNC_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;

    VIGS_LOG_TRACE("scrn_index = %d", scrn->scrnIndex);

    xf86_hide_cursors(scrn);

    scrn->vtSema = FALSE;

    vigs_drm_drop_master(vigs_screen->drm);
}

void vigs_screen_free(FREE_SCREEN_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    struct vigs_screen *vigs_screen = scrn->driverPrivate;

    VIGS_LOG_TRACE("scrn_index = %d", scrn->scrnIndex);

    if (!vigs_screen->pre_initialized) {
        return;
    }

    vigs_comm_destroy(vigs_screen->comm);

    vigs_drm_free(vigs_screen);

    vigs_screen->pre_initialized = 0;

    free(vigs_screen);
    scrn->driverPrivate = NULL;
}
