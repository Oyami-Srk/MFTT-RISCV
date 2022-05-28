#include <lib/rb_tree.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <riscv.h>

#define PARENT_MASK 0xFFFFFFFFFFFFFFFEl
#define COLOR_MASK  0x1l

#define GET_PARENT(n)                                                          \
    ((struct _rb_node *)(((struct _rb_node *)n)->parent_and_color &            \
                         PARENT_MASK))
#define SET_PARENT(n, p)                                                       \
    do {                                                                       \
        ((struct _rb_node *)n)->parent_and_color &= COLOR_MASK;                \
        ((struct _rb_node *)n)->parent_and_color |= (uint64_t)p;               \
    } while (0)
#define GET_COLOR(n)                                                           \
    (unsigned int)(((struct _rb_node *)n)->parent_and_color & COLOR_MASK)
#define SET_COLOR(n, c)                                                        \
    do {                                                                       \
        ((struct _rb_node *)n)->parent_and_color &= PARENT_MASK;               \
        ((struct _rb_node *)n)->parent_and_color |= (uint8_t)c;                \
    } while (0)
#define BLACK(n) SET_COLOR(n, RB_BLACK)
#define RED(n)   SET_COLOR(n, RB_RED)

rb_node *rb_search(rb_node *x, uint64_t key) {
    while (x != NULL) {
        if (x->key == key)
            break;
        else if (key < x->key)
            x = x->L;
        else
            x = x->R;
    }
    return x;
}

rb_node *rb_succ(rb_node *n) {
    if (n->R) {
        rb_node *x = n->R;
        while (x->L)
            x = x->L;
        return x;
    } else {
        rb_node *p = GET_PARENT(n);
        while (p && n == p->R) {
            n = p;
            p = GET_PARENT(p);
        }
        return p;
    }
}

rb_node *rb_pred(rb_node *n) {
    if (n->L) {
        rb_node *x = n->L;
        while (x->R)
            x = x->R;
        return x;
    } else {
        rb_node *p = GET_PARENT(n);
        while (p && n == p->L) {
            n = p;
            p = GET_PARENT(p);
        }
        return p;
    }
}

static ALWAYS_INLINE inline void rb_rotate_left(rb_tree *tree, rb_node *node) {
    rb_node *right  = node->R;
    rb_node *parent = GET_PARENT(node);

    if ((node->R = right->L))
        SET_PARENT(right->L, node);
    right->L = node;

    SET_PARENT(right, parent);

    if (parent) {
        if (node == parent->L)
            parent->L = right;
        else
            parent->R = right;
    } else
        tree->root = right;
    SET_PARENT(node, right);
}

static void rb_rotate_right(rb_tree *tree, rb_node *node) {
    rb_node *left   = node->L;
    rb_node *parent = GET_PARENT(node);

    if ((node->L = left->R))
        SET_PARENT(left->R, node);
    left->R = node;

    SET_PARENT(left, parent);

    if (parent) {
        if (node == parent->R)
            parent->R = left;
        else
            parent->L = left;
    } else
        tree->root = left;
    SET_PARENT(node, left);
}

static void rb_insert_fixup(rb_tree *tree, rb_node *node) {
    rb_node *parent, *gparent;

    while ((parent = GET_PARENT(node)) && GET_COLOR(parent) == RB_RED) {
        gparent = GET_PARENT(parent);

        if (parent == gparent->L) {
            {
                register rb_node *uncle = gparent->R;
                if (uncle && GET_COLOR(uncle) == RB_RED) {
                    BLACK(uncle);
                    BLACK(parent);
                    RED(gparent);
                    node = gparent;
                    continue;
                }
            }

            if (parent->R == node) {
                register rb_node *tmp;
                rb_rotate_left(tree, parent);
                tmp    = parent;
                parent = node;
                node   = tmp;
            }

            BLACK(parent);
            RED(gparent);
            rb_rotate_right(tree, gparent);
        } else {
            {
                register rb_node *uncle = gparent->L;
                if (uncle && GET_COLOR(uncle) == RB_RED) {
                    BLACK(uncle);
                    BLACK(parent);
                    RED(gparent);
                    node = gparent;
                    continue;
                }
            }

            if (parent->L == node) {
                register rb_node *tmp;
                rb_rotate_right(tree, parent);
                tmp    = parent;
                parent = node;
                node   = tmp;
            }

            BLACK(parent);
            RED(gparent);
            rb_rotate_left(tree, gparent);
        }
    }

    BLACK(tree->root);
}

rb_node *rb_insert(rb_tree *t, rb_node *n) {
    n->L = n->R = NULL;
    SET_PARENT(n, NULL);
    if (t->root == NULL) {
        BLACK(n);
        t->root = n;
    } else {
        rb_node *y = NULL;
        rb_node *x = t->root;
        while (x != NULL) {
            y = x;
            if (n->key == x->key)
                return x;
            if (n->key < x->key)
                x = x->L;
            else
                x = x->R;
        }
        SET_PARENT(n, y);
        if (n->key > y->key)
            y->R = n;
        else
            y->L = n;
        RED(n);
    }
    rb_insert_fixup(t, n);
    return NULL;
}

static void rb_remove_fixup(rb_tree *tree, rb_node *node, rb_node *parent) {
    rb_node *other;

    while ((!node || GET_COLOR(node) == RB_BLACK) && node != tree->root) {
        if (parent->L == node) {
            other = parent->R;
            if (GET_COLOR(other) == RB_RED) {
                BLACK(other);
                RED(parent);
                rb_rotate_left(tree, parent);
                other = parent->R;
            }
            if ((!other->L || GET_COLOR(other->L) == RB_BLACK) &&
                (!other->R || GET_COLOR(other->R) == RB_BLACK)) {
                RED(other);
                node   = parent;
                parent = GET_PARENT(node);
            } else {
                if (!other->R || GET_COLOR(other->R) == RB_BLACK) {
                    BLACK(other->L);
                    RED(other);
                    rb_rotate_right(tree, other);
                    other = parent->R;
                }
                SET_COLOR(other, GET_COLOR(parent));
                BLACK(parent);
                BLACK(other->R);
                rb_rotate_left(tree, parent);
                node = tree->root;
                break;
            }
        } else {
            other = parent->L;
            if (GET_COLOR(other) == RB_RED) {
                BLACK(other);
                RED(parent);
                rb_rotate_right(tree, parent);
                other = parent->L;
            }
            if ((!other->L || GET_COLOR(other->L) == RB_BLACK) &&
                (!other->R || GET_COLOR(other->R) == RB_BLACK)) {
                RED(other);
                node   = parent;
                parent = GET_PARENT(node);
            } else {
                if (!other->L || GET_COLOR(other->L) == RB_BLACK) {
                    BLACK(other->R);
                    RED(other);
                    rb_rotate_left(tree, other);
                    other = parent->L;
                }
                SET_COLOR(other, GET_COLOR(parent));
                BLACK(parent);
                BLACK(other->L);
                rb_rotate_right(tree, parent);
                node = tree->root;
                break;
            }
        }
    }
    if (node)
        BLACK(node);
}

void rb_remove(rb_tree *tree, rb_node *node) {
    rb_node *child, *parent;
    int      color;

    if (!node->L)
        child = node->R;
    else if (!node->R)
        child = node->L;
    else {
        rb_node *old = node, *left;

        node = node->R;
        while ((left = node->L) != NULL)
            node = left;

        if (GET_PARENT(old)) {
            if (GET_PARENT(old)->L == old)
                GET_PARENT(old)->L = node;
            else
                GET_PARENT(old)->R = node;
        } else
            tree->root = node;

        child  = node->R;
        parent = GET_PARENT(node);
        color  = GET_COLOR(node);

        if (parent == old) {
            parent = node;
        } else {
            if (child)
                SET_PARENT(child, parent);
            parent->L = child;

            node->R = old->R;
            SET_PARENT(old->R, node);
        }

        node->parent_and_color = old->parent_and_color;
        node->L                = old->L;
        SET_PARENT(old->L, node);

        goto color;
    }

    parent = GET_PARENT(node);
    color  = GET_COLOR(node);

    if (child)
        SET_PARENT(child, parent);
    if (parent) {
        if (parent->L == node)
            parent->L = child;
        else
            parent->R = child;
    } else
        tree->root = child;

color:
    if (color == RB_BLACK)
        rb_remove_fixup(tree, child, parent);
}

void rb_replace(rb_node *old, rb_node *new) {
    assert(old->key == new->key, "Only nodes with same key could be replace.");
    int      color  = GET_COLOR(old);
    rb_node *parent = GET_PARENT(old);
    if (old->L) {
        new->L = old->L;
        SET_PARENT(new->L, new);
    }
    if (old->R) {
        new->R = old->R;
        SET_PARENT(new->R, new);
    }
    if (parent) {
        if (parent->L == old) {
            parent->L = new;
        } else if (parent->R == old) {
            parent->R = new;
        } else {
            kpanic("Parent set but no child is us.");
        }
        SET_PARENT(new, parent);
    }
    SET_COLOR(new, color);
}