#include "vigs_comm.h"
#include "vigs_screen.h"
#include "vigs_drm.h"
#include "vigs_log.h"
#include "vigs_utils.h"
#include "vigs.h"

/*
 * Communicator's execbuffer layout:
 *
 * |----------------------------------| execbuffer->gem.vaddr
 * | batch header                     |
 * |----------------------------------|
 * | cmd 1                            |
 * |----------------------------------|
 * | ...                              |
 * |----------------------------------|
 * | cmd N-1                          |
 * |----------------------------------| cmd_ptr
 * | cmd                              |
 * |----------------------------------| cmd_ptr + cmd_size
 * |                                  |
 * |                                  |
 * |----------------------------------| execbuffer->gem.vaddr + execbuffer->gem.size
 *
 * "cmd" is the current command.
 * "cmd_ptr" is its start address.
 * "cmd_size" is the size of the current command so far.
 */

static inline int vigs_comm_have_cmds(struct vigs_comm *comm)
{
    if (comm->execbuffer &&
        (comm->cmd_ptr >
         (comm->execbuffer->gem.vaddr + sizeof(struct vigsp_cmd_batch_header)))) {
        return 1;
    } else {
        return 0;
    }
}

static inline void *vigs_comm_cmd_get_request(struct vigs_comm *comm)
{
    struct vigsp_cmd_request_header *request_header = comm->cmd_ptr;
    return request_header + 1;
}

/*
 * Tries to allocate additional 'size' bytes for current command,
 * returns a pointer to newly allocated storage.
 */
static void *vigs_comm_execbuffer_realloc(struct vigs_comm *comm, uint32_t size)
{
    int ret;
    struct vigs_drm_execbuffer *new_execbuffer;
    uint32_t complete_size = 0, new_size;
    void *tmp;

    if (comm->execbuffer) {
        complete_size = (comm->cmd_ptr - comm->execbuffer->gem.vaddr);
    }

    new_size = complete_size + comm->cmd_size + size;

    if (comm->execbuffer && (new_size <= comm->execbuffer->gem.size)) {
        tmp = comm->cmd_ptr + comm->cmd_size;
        comm->cmd_size += size;
        return tmp;
    }

    if (new_size > comm->max_size) {
        xf86DrvMsg(comm->screen->scrn->scrnIndex, X_WARNING, "Not allocating execbuffer of %u bytes, max allowed is %u\n", new_size, comm->max_size);
        return NULL;
    }

    ret = vigs_drm_execbuffer_create(comm->screen->drm->dev,
                                     new_size,
                                     &new_execbuffer);

    if (ret != 0) {
        xf86DrvMsg(comm->screen->scrn->scrnIndex, X_ERROR, "Unable to allocate %u byte execbuffer: %s\n", new_size, strerror(-ret));
        return NULL;
    }

    VIGS_LOG_TRACE("Allocated %u byte execbuffer", new_execbuffer->gem.size);

    ret = vigs_drm_gem_map(&new_execbuffer->gem, 0);

    if (ret != 0) {
        xf86DrvMsg(comm->screen->scrn->scrnIndex, X_ERROR, "Unable to map execbuffer\n");
        vigs_drm_gem_unref(&new_execbuffer->gem);
        return NULL;
    }

    if (comm->execbuffer) {
        memcpy(new_execbuffer->gem.vaddr,
               comm->execbuffer->gem.vaddr,
               complete_size + comm->cmd_size);
        vigs_drm_gem_unref(&comm->execbuffer->gem);
    }

    comm->execbuffer = new_execbuffer;

    comm->cmd_ptr = comm->execbuffer->gem.vaddr + complete_size;

    tmp = comm->cmd_ptr + comm->cmd_size;

    comm->cmd_size += size;

    return tmp;
}

/*
 * Tries to allocate additional 'size' bytes for current command,
 * returns a pointer to newly allocated storage. Flushes complete
 * commands on memory shortage.
 */
static void *vigs_comm_cmd_alloc(struct vigs_comm *comm, uint32_t size)
{
    void *tmp = vigs_comm_execbuffer_realloc(comm, size);

    if (tmp) {
        return tmp;
    }

    if (!vigs_comm_have_cmds(comm)) {
        /*
         * Failed to allocate additional space in execbuffer, if the current
         * command is the only one in the buffer then we obviously failed.
         */
        comm->alloc_failed = 1;
        return NULL;
    }

    /*
     * Flush complete commands and try again.
     * We don't care about the result of the flush in this case.
     */

    (void)vigs_comm_flush(comm);

    tmp = vigs_comm_execbuffer_realloc(comm, size);

    if (!tmp) {
        comm->alloc_failed = 1;
    }

    return tmp;
}

static void *vigs_comm_cmd_prepare(struct vigs_comm *comm,
                                   vigsp_cmd cmd,
                                   uint32_t request_size)
{
    struct vigsp_cmd_batch_header *batch_header;
    struct vigsp_cmd_request_header *request_header;

    if (comm->cmd_size > 0) {
        xf86DrvMsg(comm->screen->scrn->scrnIndex, X_ERROR, "Another command already in progress, logic error\n");
        return NULL;
    }

    if (comm->execbuffer) {
        request_header = vigs_comm_cmd_alloc(comm,
            sizeof(*request_header) +
            request_size);

        if (!request_header) {
            comm->alloc_failed = 0;
            return NULL;
        }
    } else {
        /*
         * First allocation.
         */
        batch_header = vigs_comm_cmd_alloc(comm,
            sizeof(struct vigsp_cmd_batch_header) +
            sizeof(*request_header) +
            request_size);

        if (!batch_header) {
            comm->alloc_failed = 0;
            return NULL;
        }

        request_header = (struct vigsp_cmd_request_header*)(batch_header + 1);

        /*
         * Don't account for batch header as part of command.
         */
        comm->cmd_size -= sizeof(struct vigsp_cmd_batch_header);
    }

    comm->cmd = cmd;
    comm->cmd_ptr = request_header;

    return request_header + 1;
}

static void *vigs_comm_cmd_append(struct vigs_comm *comm,
                                  vigsp_cmd cmd,
                                  uint32_t size)
{
    if (comm->cmd_size <= 0) {
        return NULL;
    }

    if (cmd != comm->cmd) {
        xf86DrvMsg(comm->screen->scrn->scrnIndex, X_ERROR, "Another command already in progress, logic error\n");
        return NULL;
    }

    if (comm->alloc_failed) {
        VIGS_LOG_ERROR("Allocation failed, not appending");
        return NULL;
    }

    return vigs_comm_cmd_alloc(comm, size);
}

static void vigs_comm_cmd_done(struct vigs_comm *comm,
                               vigsp_cmd cmd)
{
    struct vigsp_cmd_request_header *request_header;

    if (comm->cmd_size <= 0) {
        return;
    }

    if (cmd != comm->cmd) {
        xf86DrvMsg(comm->screen->scrn->scrnIndex, X_ERROR, "Another command already in progress, logic error\n");
        return;
    }

    if (comm->alloc_failed) {
        VIGS_LOG_ERROR("Allocation failed, not executing");
        comm->cmd_size = 0;
        comm->alloc_failed = 0;
        return;
    }

    request_header = comm->cmd_ptr;

    request_header->cmd = comm->cmd;
    request_header->size = comm->cmd_size - sizeof(*request_header);

    comm->cmd_ptr += comm->cmd_size;
    comm->cmd_size = 0;
}

Bool vigs_comm_create(struct vigs_screen *vigs_screen,
                      uint32_t max_size,
                      struct vigs_comm **comm)
{
    uint32_t protocol_version;
    int ret;

    VIGS_LOG_TRACE("enter");

    ret = vigs_drm_device_get_protocol_version(vigs_screen->drm->dev,
                                               &protocol_version);

    if (ret != 0) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to get VIGS protocol version from kernel: %s\n",
                   strerror(-ret));
        goto fail1;
    }

    if (protocol_version != VIGS_PROTOCOL_VERSION) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "VIGS protocol version mismatch: actual %u, expected %u\n",
                   protocol_version, VIGS_PROTOCOL_VERSION);
        goto fail1;
    }

    *comm = calloc(sizeof(**comm), 1);

    if (!*comm) {
        xf86DrvMsg(vigs_screen->scrn->scrnIndex, X_ERROR, "Unable to allocate VIGS communicator\n");
        goto fail1;
    }

    (*comm)->screen = vigs_screen;
    (*comm)->max_size = max_size;

    return TRUE;

fail1:
    *comm = NULL;

    return FALSE;
}

void vigs_comm_destroy(struct vigs_comm *comm)
{
    VIGS_LOG_TRACE("enter");

    if (comm->execbuffer) {
        vigs_drm_gem_unref(&comm->execbuffer->gem);
    }
    free(comm);
}

Bool vigs_comm_flush(struct vigs_comm *comm)
{
    struct vigsp_cmd_batch_header *batch_header;
    int ret;
    Bool result = TRUE;

    if (!vigs_comm_have_cmds(comm)) {
        return TRUE;
    }

    batch_header = comm->execbuffer->gem.vaddr;

    batch_header->fence_seq = 0;
    batch_header->size = (comm->cmd_ptr -
                          comm->execbuffer->gem.vaddr -
                          sizeof(*batch_header));

    ret = vigs_drm_execbuffer_exec(comm->execbuffer);
    if (ret != 0) {
        xf86DrvMsg(comm->screen->scrn->scrnIndex, X_ERROR, "Failed to execute execbuffer: %s\n", strerror(-ret));
        result = FALSE;
    }

    memcpy(batch_header + 1, comm->cmd_ptr, comm->cmd_size);

    comm->cmd_ptr = batch_header + 1;

    return result;
}

void vigs_comm_update_vram(struct vigs_comm *comm,
                           vigsp_surface_id sfc_id)
{
    struct vigsp_cmd_update_vram_request *request;

    request = vigs_comm_cmd_prepare(comm,
                                    vigsp_cmd_update_vram,
                                    sizeof(*request));

    if (!request) {
        return;
    }

    request->sfc_id = sfc_id;

    vigs_comm_cmd_done(comm, vigsp_cmd_update_vram);
}

void vigs_comm_update_gpu(struct vigs_comm *comm,
                          vigsp_surface_id sfc_id,
                          RegionPtr region)
{
    BoxPtr entries = REGION_RECTS(region);
    uint32_t num_entries = REGION_NUM_RECTS(region);
    struct vigsp_cmd_update_gpu_request *request;
    uint32_t i;

    request = vigs_comm_cmd_prepare(comm,
                                    vigsp_cmd_update_gpu,
                                    sizeof(*request) +
                                    (sizeof(struct vigsp_rect) * num_entries));

    if (!request) {
        return;
    }

    request->sfc_id = sfc_id;
    request->num_entries = num_entries;

    for (i = 0; i < num_entries; ++i) {
        request->entries[i].pos.x = entries[i].x1;
        request->entries[i].pos.y = entries[i].y1;
        request->entries[i].size.w = (entries[i].x2 - entries[i].x1);
        request->entries[i].size.h = (entries[i].y2 - entries[i].y1);
    }

    vigs_comm_cmd_done(comm, vigsp_cmd_update_gpu);
}

void vigs_comm_copy_prepare(struct vigs_comm *comm,
                            vigsp_surface_id src_id,
                            vigsp_surface_id dst_id)
{
    struct vigsp_cmd_copy_request *request;

    request = vigs_comm_cmd_prepare(comm,
                                    vigsp_cmd_copy,
                                    sizeof(*request));

    if (!request) {
        return;
    }

    request->src_id = src_id;
    request->dst_id = dst_id;
    request->num_entries = 0;
}

void vigs_comm_copy(struct vigs_comm *comm,
                    int src_x1, int src_y1,
                    int dst_x1, int dst_y1,
                    int w, int h)
{
    struct vigsp_cmd_copy_request *request;
    struct vigsp_copy *entry;

    entry = vigs_comm_cmd_append(comm,
                                 vigsp_cmd_copy,
                                 sizeof(*entry));

    if (!entry) {
        return;
    }

    entry->from.x = src_x1;
    entry->from.y = src_y1;
    entry->to.x = dst_x1;
    entry->to.y = dst_y1;
    entry->size.w = w;
    entry->size.h = h;

    request = vigs_comm_cmd_get_request(comm);

    ++request->num_entries;
}

void vigs_comm_copy_done(struct vigs_comm *comm)
{
    vigs_comm_cmd_done(comm, vigsp_cmd_copy);
}

void vigs_comm_solid_fill_prepare(struct vigs_comm *comm,
                                  vigsp_surface_id sfc_id,
                                  vigsp_color color)
{
    struct vigsp_cmd_solid_fill_request *request;

    request = vigs_comm_cmd_prepare(comm,
                                    vigsp_cmd_solid_fill,
                                    sizeof(*request));

    if (!request) {
        return;
    }

    request->sfc_id = sfc_id;
    request->color = color;
    request->num_entries = 0;
}

void vigs_comm_solid_fill(struct vigs_comm *comm,
                          int x1, int y1,
                          int x2, int y2)
{
    struct vigsp_cmd_solid_fill_request *request;
    struct vigsp_rect *entry;

    entry = vigs_comm_cmd_append(comm,
                                 vigsp_cmd_solid_fill,
                                 sizeof(*entry));

    if (!entry) {
        return;
    }

    assert((x2 - x1) > 0);
    assert((y2 - y1) > 0);

    entry->pos.x = x1;
    entry->pos.y = y1;
    entry->size.w = x2 - x1;
    entry->size.h = y2 - y1;

    request = vigs_comm_cmd_get_request(comm);

    ++request->num_entries;
}

void vigs_comm_solid_fill_done(struct vigs_comm *comm)
{
    vigs_comm_cmd_done(comm, vigsp_cmd_solid_fill);
}
