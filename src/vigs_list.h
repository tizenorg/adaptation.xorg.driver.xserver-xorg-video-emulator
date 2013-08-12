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
