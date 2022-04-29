//
// Created by shiroko on 22-4-22.
//

#ifndef __RB_TREE_H__
#define __RB_TREE_H__

#include <common/types.h>

struct _rb_node {
    uint64_t key;
    uint64_t parent_and_color;
#define RB_BLACK 0
#define RB_RED   1
    struct _rb_node *L;
    struct _rb_node *R;
} __attribute__((aligned(16)));

struct _rb_tree {
    struct _rb_node *root;
};
typedef struct _rb_node rb_node;
typedef struct _rb_tree rb_tree;

rb_node *rb_search(rb_node *x, uint64_t key);
int      rb_insert(rb_tree *t, rb_node *n);
void     rb_remove(rb_tree *t, rb_node *n);
rb_node *rb_succ(rb_node *n);
rb_node *rb_pred(rb_node *n);

#endif // __RB_TREE_H__