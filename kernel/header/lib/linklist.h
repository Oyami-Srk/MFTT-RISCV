//
// Created by shiroko on 22-5-4.
//

#ifndef __LIB_LINKLIST_H__
#define __LIB_LINKLIST_H__

#include <lib/stdlib.h>

// Linux Linklist

typedef struct __list_head_t {
    struct __list_head_t *next, *prev;
} list_head_t;

#define LIST_HEAD_INIT(name)                                                   \
    { &(name), &(name) }
#define LIST_HEAD(name) list_head_t name = LIST_HEAD_INIT(name)

// insert new between prev and next
static inline void __list_add(list_head_t *new, list_head_t *prev,
                              list_head_t *next) {
    next->prev = new;
    new->next  = next;
    prev->next = new;
    new->prev  = prev;
}

#define list_add(new, head)      __list_add(new, (head), (head)->next)
#define list_add_tail(new, head) __list_add(new, (head)->prev, (head))

static inline void list_del(list_head_t *node) {
    list_head_t *prev = node->prev;
    list_head_t *next = node->next;
    prev->next        = next;
    next->prev        = prev;
    node->next        = NULL;
    node->next        = NULL;
}

// head is header pointer
#define list_foreach_entry(head, container_type, container_member, entry)      \
    for (container_type *entry =                                               \
             container_of((head)->next, container_type, container_member);     \
         &entry->container_member != (head);                                   \
         entry = container_of(entry->container_member.next, container_type,    \
                              container_member))

#endif // __LIB_LINKLIST_H__