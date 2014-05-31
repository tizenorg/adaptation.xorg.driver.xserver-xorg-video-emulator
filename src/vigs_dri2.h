#ifndef _VIGS_DRI2_H_
#define _VIGS_DRI2_H_

#include "vigs_config.h"
#include "vigs_list.h"
#include "xf86.h"
#include "dri2.h"

struct vigs_screen;

typedef enum
{
    vigs_dri2_swap,
    vigs_dri2_flip,
    vigs_dri2_waitmsc
} vigs_dri2_frame_event_type;

struct vigs_dri2_frame_event
{
    /* Screen we're on. */
    struct vigs_screen *screen;

    /* Link for vigs_dri2_client::frame_events. */
    struct vigs_list list;

    /* Client that owns this frame event. NULL if client left. */
    ClientPtr client;

    /*
     * XID of a drawable we operate on. We can't
     * store 'DrawablePtr' here because the drawable
     * might be gone by the time we enter swap/flip
     * handler.
     */
    XID drawable_id;

    /* Frame event type. */
    vigs_dri2_frame_event_type type;

    unsigned int sequence;

    /*
     * Non NULL only for swap/flip.
     * Becomes NULL when client leaves. This is identified by
     * 'client' is also being NULL.
     * @{
     */
    DRI2SwapEventPtr event_func;
    void *event_data;
    DRI2BufferPtr src;
    DRI2BufferPtr dest;
    /*
     * @}
     */
};

Bool vigs_dri2_init(struct vigs_screen *vigs_screen);

void vigs_dri2_close(struct vigs_screen *vigs_screen);

void vigs_dri2_vblank_handler(struct vigs_dri2_frame_event *frame_event,
                              unsigned int sequence,
                              unsigned int tv_sec,
                              unsigned int tv_usec);

void vigs_dri2_page_flip_handler(struct vigs_dri2_frame_event *frame_event,
                                 unsigned int sequence,
                                 unsigned int tv_sec,
                                 unsigned int tv_usec);

#endif
