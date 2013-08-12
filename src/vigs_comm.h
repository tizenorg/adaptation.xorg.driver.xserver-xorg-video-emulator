#ifndef _VIGS_COMM_H_
#define _VIGS_COMM_H_

#include "vigs_config.h"
#include "xf86.h"
#include "vigs_protocol.h"

struct vigs_screen;
struct vigs_drm_execbuffer;

struct vigs_comm
{
    /* Screen we're on. */
    struct vigs_screen *screen;

    /* Execbuffer will not grow beyond this. */
    uint32_t max_size;

    /* Execbuffer that holds command data. */
    struct vigs_drm_execbuffer *execbuffer;

    /* Number of complete commands in execbuffer. */
    uint32_t cmd_count;

    /* Current command. */
    vigsp_cmd cmd;

    /* Pointer to start of current command. */
    void *cmd_ptr;

    /* Command size so far. */
    uint32_t cmd_size;

    /* Allocation failed, just reset the whole thing when command is done. */
    int alloc_failed;

    /*
     * Temporary buffer for keeping incomplete command
     * until flush is processed.
     * @{
     */
    void *tmp_buff;
    uint32_t tmp_buff_size;
    /*
     * @}
     */
};

Bool vigs_comm_create(struct vigs_screen *vigs_screen,
                      uint32_t max_size,
                      struct vigs_comm **comm);

void vigs_comm_destroy(struct vigs_comm *comm);

Bool vigs_comm_flush(struct vigs_comm *comm);

void vigs_comm_update_vram(struct vigs_comm *comm,
                           vigsp_surface_id sfc_id);

void vigs_comm_update_gpu(struct vigs_comm *comm,
                          vigsp_surface_id sfc_id,
                          RegionPtr region);

void vigs_comm_copy_prepare(struct vigs_comm *comm,
                            vigsp_surface_id src_id,
                            vigsp_surface_id dst_id);

void vigs_comm_copy(struct vigs_comm *comm,
                    int src_x1, int src_y1,
                    int dst_x1, int dst_y1,
                    int w, int h);

void vigs_comm_copy_done(struct vigs_comm *comm);

void vigs_comm_solid_fill_prepare(struct vigs_comm *comm,
                                  vigsp_surface_id sfc_id,
                                  vigsp_color color);

void vigs_comm_solid_fill(struct vigs_comm *comm,
                          int x1, int y1,
                          int x2, int y2);

void vigs_comm_solid_fill_done(struct vigs_comm *comm);

#endif
