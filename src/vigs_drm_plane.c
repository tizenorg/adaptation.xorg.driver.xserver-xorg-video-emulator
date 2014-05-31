#include "vigs_drm_plane.h"
#include "vigs_drm.h"
#include "vigs_log.h"

Bool vigs_drm_plane_init(struct vigs_drm *drm, int num)
{
    struct vigs_drm_plane *plane;

    VIGS_LOG_TRACE("%d", num);

    plane = xnfcalloc(sizeof(*plane), 1);

    vigs_list_init(&plane->list);
    plane->drm = drm;
    plane->num = num;
    plane->mode_plane = drmModeGetPlane(drm->fd,
                                        drm->plane_res->planes[num]);

    if (!plane->mode_plane) {
        return FALSE;
    }

    vigs_list_add_tail(&drm->planes, &plane->list);

    return TRUE;
}

void vigs_drm_plane_destroy(struct vigs_drm_plane *plane)
{
    VIGS_LOG_TRACE("%d", plane->num);

    vigs_list_remove(&plane->list);

    drmModeFreePlane(plane->mode_plane);

    free(plane);
}
