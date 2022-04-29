//
// Created by shiroko on 22-4-25.
//

#include "./utils.h"
#include <driver/console.h>
#include <lib/rb_tree.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <memory.h>

struct __kmem_pool {
    size_t              total_size;
    size_t              total_free;
    struct __kmem_pool *next_pool;
    rb_tree             free_tree;
};

typedef struct __kmem_pool kmem_pool;

struct kmem_block {
    uint32_t cookie;
    // We assume there is nothing need larger than 2^32-1 bytes.
    uint32_t size; // size is memory block size, not include header.
#define COOKIE_MEM_INUSE_BLOCK 0x0020D1AC
#define COOKIE_MEM_FREE_BLOCK  0xFF20D1AC
#define COOKIE_MEM_NEXT_BLOCK  0x1120D1AC
    kmem_pool *pool;
    union {
        char              *mem;
        struct kmem_block *next_free;
    } mem;
} __attribute__((aligned(16)));

// const size_t kmem_block_head_size = sizeof(struct kmem_block);
const size_t kmem_block_head_size =
    sizeof(uint32_t) + sizeof(uint32_t) + sizeof(struct kmem_block *);
const size_t min_block_size = sizeof(struct kmem_block) * 2;
const size_t max_block_size = (1 << (MAX_BUDDY_ORDER - 1)) * PG_SIZE;

// rbtree->key is size, include struct itself

kmem_pool *memory_pool;

static kmem_pool *insert_to_pool(kmem_pool *pool, char *p, size_t size) {
    memset(p, 0, sizeof(rb_node));
    ((rb_node *)p)->key = size;
    int is_key_exists   = rb_insert(&pool->free_tree, (rb_node *)p);
    if (is_key_exists) {
        struct kmem_block *mb = (struct kmem_block *)p;
        memset(mb, 0, sizeof(struct kmem_block));

        rb_node           *node = rb_search(pool->free_tree.root, size);
        struct kmem_block *node_next =
            (struct kmem_block *)(node + sizeof(rb_node));
        if (node_next->pool != pool) {
            kpanic("mem pool not match");
        }

        mb->cookie = COOKIE_MEM_FREE_BLOCK;
        mb->size   = size - kmem_block_head_size;
        mb->pool   = pool;

        // insert to free list of this size
        if (node_next->cookie != COOKIE_MEM_NEXT_BLOCK) {
            node_next->cookie        = COOKIE_MEM_NEXT_BLOCK;
            node_next->mem.next_free = mb;
            mb->mem.next_free        = NULL;
        } else {
            mb->mem.next_free        = node_next->mem.next_free;
            node_next->mem.next_free = mb;
        }
    }
    return pool;
}

// remove an existing node and put it successor
// trust p as a valid node in pool's tree,
// so we can just save O(log2(n)) for this func
static kmem_pool *remove_from_pool(kmem_pool *pool, rb_node *p) {
    struct kmem_block *next_block = (struct kmem_block *)(p + sizeof(rb_node));
    rb_remove(&pool->free_tree, p);
    if (next_block->cookie == COOKIE_MEM_NEXT_BLOCK) {
        struct kmem_block *mb = next_block->mem.next_free;
        if (mb->cookie != COOKIE_MEM_FREE_BLOCK) {
            kpanic("mem cookie not match to free block");
        }
        struct kmem_block *mb_next = mb->mem.next_free;
        memset(mb, 0, sizeof(struct kmem_block));
        ((rb_node *)mb)->key = mb->size + kmem_block_head_size;
        rb_insert(&pool->free_tree, (rb_node *)mb);
        struct kmem_block *mb_n = (struct kmem_block *)(mb + sizeof(rb_node));
        memset(mb_n, 0, sizeof(struct kmem_block));

        if (mb_next) {
            mb_n->cookie        = COOKIE_MEM_NEXT_BLOCK;
            mb_n->mem.next_free = mb_next;
        }
    }
    return pool;
}

static kmem_pool *create_a_mem_pool(size_t pages) {
    kmem_pool *pool = NULL;
    pool = (kmem_pool *)page_alloc(pages, PAGE_TYPE_INUSE | PAGE_TYPE_POOL);
    pool->total_size     = pages * PG_SIZE;
    pool->total_free     = pool->total_size - sizeof(kmem_pool);
    pool->next_pool      = NULL;
    pool->free_tree.root = NULL;
    insert_to_pool(pool, (char *)pool + sizeof(kmem_pool), pool->total_free);
    return pool;
}

// pool_size is aligned to PG_SIZE
void init_memory_pool(size_t pool_size) {
    size_t     pages = (pool_size + PG_SIZE - 1) / PG_SIZE;
    kmem_pool *pools = NULL;
    while (pages) {
        size_t p2pg = round_down_power_2(pages);
        if (pools)
            pools->next_pool = create_a_mem_pool(p2pg);
        else
            pools = create_a_mem_pool(p2pg);
        pages -= p2pg;
    }
    memory_pool = pools; // set global varible
}

static rb_node *rb_search_upper(rb_node *x, size_t key) {
    rb_node *closet = NULL;
    while (x != NULL) {
        if (x->key == key)
            break;
        if (x->key > key) {
            if (!closet || closet->key > x->key)
                closet = x;
            x = x->L;
        } else {
            x = x->R;
        }
    }
    if (x != NULL)
        return x;
    else
        return closet;
}

char *kmalloc(size_t size) {
    size += kmem_block_head_size;
    size                 = ROUNDUP_WITH(16, size); // 16 byte aligned
    size_t     used_size = size;
    kmem_pool *pool      = memory_pool;
    rb_node   *node      = NULL;
    while (pool && !node) {
        node = rb_search_upper(pool->free_tree.root, size);
        if (node == NULL)
            pool = pool->next_pool;
    }
    if (node == NULL)
        return NULL;
    remove_from_pool(memory_pool, node);

    size_t remaining_size = 0;
    if (node->key - size < 16)
        used_size = node->key;
    else
        remaining_size = node->key - size;

    if (remaining_size) {
        insert_to_pool(pool, (char *)(node + size), remaining_size);
    }

    struct kmem_block *mb = (struct kmem_block *)node;
    memset(mb, 0, sizeof(struct kmem_block));
    mb->pool   = pool;
    mb->size   = used_size;
    mb->cookie = COOKIE_MEM_INUSE_BLOCK;
    // return (char *)(node + kmem_block_head_size); // this code should be
    // equal to below one
    return (char *)(&mb->mem.mem);
}

void kfree(void *p) {
    struct kmem_block *mb = (struct kmem_block *)(p - kmem_block_head_size);
    if (mb->cookie != COOKIE_MEM_INUSE_BLOCK) {
        kpanic("Kmem block cookie not match");
        return;
    }

    // found if this block is next to a free block
    char  *next_block      = p + mb->size;
    bool   next_is_free    = false;
    size_t next_block_size = 0;
    if (next_block < (char *)mb->pool + mb->pool->total_size) {
        if (((struct kmem_block *)next_block)->cookie ==
            COOKIE_MEM_FREE_BLOCK) {
            next_is_free = true;
            next_block_size =
                ((struct kmem_block *)next_block)->size + kmem_block_head_size;
        } else if (rb_search(mb->pool->free_tree.root,
                             ((rb_node *)next_block)->key) ==
                   (rb_node *)next_block) {
            next_is_free    = true;
            next_block_size = ((rb_node *)next_block)->key;
        }
    }

    if (!next_is_free) {
        insert_to_pool(mb->pool, (char *)mb, mb->size + kmem_block_head_size);
    } else {
        mb->size += next_block_size;
        kfree(p);
    }
}
