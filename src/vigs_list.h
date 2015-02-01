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

#ifndef _VIGS_LIST_H_
#define _VIGS_LIST_H_

#include "vigs_config.h"
#include "vigs_utils.h"

struct vigs_list
{
    struct vigs_list* prev;
    struct vigs_list* next;
};

/*
 * Private interface.
 */

static __inline void __vigs_list_add( struct vigs_list* nw,
                                      struct vigs_list* prev,
                                      struct vigs_list* next )
{
    next->prev = nw;
    nw->next = next;
    nw->prev = prev;
    prev->next = nw;
}

static __inline void __vigs_list_remove( struct vigs_list* prev,
                                         struct vigs_list* next )
{
    next->prev = prev;
    prev->next = next;
}

/*
 * Public interface.
 */

#define VIGS_DECLARE_LIST(name) struct vigs_list name = { &(name), &(name) }

static __inline void vigs_list_init(struct vigs_list* list)
{
    list->next = list;
    list->prev = list;
}

static __inline void vigs_list_add(struct vigs_list* head, struct vigs_list* nw)
{
    __vigs_list_add(nw, head, head->next);
}

static __inline void vigs_list_add_tail(struct vigs_list* head, struct vigs_list* nw)
{
    __vigs_list_add(nw, head->prev, head);
}

static __inline void vigs_list_remove(struct vigs_list* entry)
{
    __vigs_list_remove(entry->prev, entry->next);
    vigs_list_init(entry);
}

static __inline int vigs_list_empty(const struct vigs_list* head)
{
    return ( (head->next == head) && (head->prev == head) );
}

#define vigs_list_first(container_type, iter, head, member) iter = vigs_containerof((head)->next, container_type, member)

#define vigs_list_last(container_type, iter, head, member) iter = vigs_containerof((head)->prev, container_type, member)

/*
 * Iterate over list in direct and reverse order.
 */

#define vigs_list_for_each(container_type, iter, head, member) \
    for ( iter = vigs_containerof((head)->next, container_type, member); \
          &iter->member != (head); \
          iter = vigs_containerof(iter->member.next, container_type, member) )

#define vigs_list_for_each_reverse(container_type, iter, head, member) \
    for ( iter = vigs_containerof((head)->prev, container_type, member); \
          &iter->member != (head); \
          iter = vigs_containerof(iter->member.prev, container_type, member) )

/*
 * Iterate over list in direct and reverse order, safe to list entries removal.
 */

#define vigs_list_for_each_safe(container_type, iter, tmp_iter, head, member) \
    for ( iter = vigs_containerof((head)->next, container_type, member), \
          tmp_iter = vigs_containerof(iter->member.next, container_type, member); \
          &iter->member != (head); \
          iter = tmp_iter, tmp_iter = vigs_containerof(tmp_iter->member.next, container_type, member) )

#define vigs_list_for_each_safe_reverse(container_type, iter, tmp_iter, head, member) \
    for ( iter = vigs_containerof((head)->prev, container_type, member), \
          tmp_iter = vigs_containerof(iter->member.prev, container_type, member); \
          &iter->member != (head); \
          iter = tmp_iter, tmp_iter = vigs_containerof(tmp_iter->member.prev, container_type, member) )

#endif
