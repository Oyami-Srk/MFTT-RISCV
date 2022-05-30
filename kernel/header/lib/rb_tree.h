//
// Created by shiroko on 22-4-22.
//

#ifndef __RB_TREE_H__
#define __RB_TREE_H__

#include <types.h>

#define RB_BLACK 0
#define RB_RED   1

struct _rb_node {
    uint64_t         key;
    uint64_t         parent_and_color;
    struct _rb_node *L;
    struct _rb_node *R;
} __attribute__((aligned(8)));

struct _rb_tree {
    struct _rb_node *root;
};
typedef struct _rb_node rb_node;
typedef struct _rb_tree rb_tree;

// Read
// Search for given key, return node.
rb_node *rb_search(rb_node *x, uint64_t key);
// Get next larger node
rb_node *rb_succ(rb_node *n);
// Get previous smaller node
rb_node *rb_pred(rb_node *n);

// Write
// Insert a node, return NULL if success otherwise node that already existed.
rb_node *rb_insert(rb_tree *t, rb_node *n);
// Remove a node, n must inside the tree
void rb_remove(rb_tree *t, rb_node *n);
// Replace the old with new, key must be identical
void rb_replace(rb_node *old, rb_node *new);

#endif // __RB_TREE_H__