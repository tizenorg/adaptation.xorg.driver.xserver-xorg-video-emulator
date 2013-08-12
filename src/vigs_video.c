#include "vigs_video.h"
#include "vigs_video_out.h"
#include "vigs_video_in.h"
#include "vigs_screen.h"
#include "vigs_video_adaptor.h"
#include "vigs_video_overlay.h"
#include "vigs_log.h"
#include <X11/Xatom.h>

static const char *g_overlay_device_names[VIGS_NUM_VIDEO_OVERLAYS] =
{
    "/dev/video1",
    "/dev/video2",
};

static Bool g_block_handler_registered = FALSE;

static Bool vigs_video_set_hw_ports_property(ScreenPtr screen, int num)
{
    WindowPtr window = screen->root;
    Atom atom;

    if (!window || !serverClient) {
        return FALSE;
    }

    atom = MakeAtom("X_HW_PORTS", strlen("X_HW_PORTS"), TRUE);

    dixChangeWindowProperty(serverClient,
                            window, atom, XA_CARDINAL, 32,
                            PropModeReplace, 1, (unsigned int*)&num, FALSE);

    return TRUE;
}

static void vigs_video_block_handler(pointer data,
                                     OSTimePtr timeout,
                                     pointer read)
{
    ScrnInfoPtr scrn = (ScrnInfoPtr)data;

    if (g_block_handler_registered &&
        vigs_video_set_hw_ports_property(scrn->pScreen, VIGS_NUM_VIDEO_OVERLAYS))
    {
        RemoveBlockAndWakeupHandlers(&vigs_video_block_handler,
                                     (WakeupHandlerProcPtr)NoopDDA,
                                     data);
        g_block_handler_registered = FALSE;
    }
}

Bool vigs_video_init(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    struct vigs_video *xv;
    struct vigs_video_adaptor *adaptor;

    VIGS_LOG_TRACE("enter");

    xv = vigs_screen->xv = xnfcalloc(sizeof(*xv), 1);

    xv->screen = vigs_screen;

    adaptor = vigs_video_out_create(xv);

    if (!adaptor) {
        return FALSE;
    }

    xv->adaptors[0] = &adaptor->base;

    adaptor = vigs_video_in_create(xv);

    if (!adaptor) {
        return FALSE;
    }

    xv->adaptors[1] = &adaptor->base;

    if (!xf86XVScreenInit(scrn->pScreen, &xv->adaptors[0], VIGS_NUM_VIDEO_ADAPTORS)) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "xf86XVScreenInit failed\n");
        return FALSE;
    }

    if (!g_block_handler_registered) {
        RegisterBlockAndWakeupHandlers(&vigs_video_block_handler,
                                       (WakeupHandlerProcPtr)NoopDDA,
                                       scrn);
        g_block_handler_registered = TRUE;
    }

    VIGS_LOG_TRACE("Xv initialized");

    return TRUE;
}

void vigs_video_close(struct vigs_screen *vigs_screen)
{
    ScrnInfoPtr scrn = vigs_screen->scrn;
    struct vigs_video *xv = vigs_screen->xv;
    int i;

    VIGS_LOG_TRACE("enter");

    for (i = 0; i < VIGS_NUM_VIDEO_ADAPTORS; ++i) {
        struct vigs_video_adaptor *adaptor = (struct vigs_video_adaptor*)xv->adaptors[i];

        adaptor->destroy(adaptor);
        xv->adaptors[i] = NULL;
    }

    for (i = 0; i < VIGS_NUM_VIDEO_OVERLAYS; ++i) {
        if (!xv->overlays[i]) {
            continue;
        }

        xf86DrvMsg(scrn->scrnIndex, X_WARNING, "Overlay \"%s\" is still opened after all adapter have been closed\n",
                   xv->overlays[i]->device_name);

        vigs_video_close_overlay(xv, i);
    }

    free(xv);
    vigs_screen->xv = NULL;
}

int vigs_video_open_overlay(struct vigs_video *xv)
{
    int i;

    for (i = 0; i < VIGS_NUM_VIDEO_OVERLAYS; ++i) {
        if (!xv->overlays[i]) {
            xv->overlays[i] =
                vigs_video_overlay_create(xv->screen,
                                          g_overlay_device_names[i]);
            return xv->overlays[i] ? i : -1;
        }
    }

    return -1;
}

void vigs_video_close_overlay(struct vigs_video *xv, int overlay_index)
{
    assert((overlay_index >= 0) && (overlay_index < VIGS_NUM_VIDEO_OVERLAYS));

    if (!xv->overlays[overlay_index]) {
        xf86DrvMsg(xv->screen->scrn->scrnIndex, X_ERROR, "Overlay %d not opened\n", overlay_index);
        return;
    }

    vigs_video_overlay_destroy(xv->overlays[overlay_index]);
    xv->overlays[overlay_index] = NULL;
}
