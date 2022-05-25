//
// Created by shiroko on 22-4-25.
//

#include "./utils.h"
#include <driver/console.h>
#include <lib/rb_tree.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/spinlock.h>
#include <memory.h>

struct __kmem_pool {
    size_t              free;
    size_t              size; // not include header
    struct __kmem_pool *next_pool;
    rb_tree             free_tree;
    spinlock_t          lock;
    char                mem[0];
};

typedef struct __kmem_pool kmem_pool;
static spinlock_t          kmem_lock = {.lock = false, .cpu = 0};

// TODO: Smaller the block, glibc is amazing at pool management
struct __kmem_block {
    uint16_t cookie;
#define KMEM_COOKIE 0xD1AC
    uint16_t type;
#define KMEM_TYPE_FREE      0x0001
#define KMEM_TYPE_INUSE     0x1000
#define KMEM_TYPE_HAVE_NEXT 0x0002
#define KMEM_TYPE_IN_TREE   0x0004
    // We assume there is nothing need larger than 2^32-1 bytes.
    uint32_t   size; // size is memory block size, not include header.
    kmem_pool *pool;
    union __mem {
        char mem[0];
        struct {
            struct __kmem_block *prev_free;
            struct __kmem_block *next_free;
        } list;
    } mem;
    rb_node tree_node;
} __attribute__((aligned(4)));
typedef struct __kmem_block kmem_block;

const size_t kmem_block_head_size =
    sizeof(uint16_t) * 2 + sizeof(uint32_t) + sizeof(kmem_pool *);

// rbtree->key is size, include struct itself

kmem_pool        *memory_pool      = NULL;
static spinlock_t global_pool_lock = {.lock = 0, .cpu = 0};

static void insert_into_pool(kmem_pool *pool, char *mem, size_t sz);
static void remove_from_pool(kmem_block *block);

void attach_to_memory_pool(char *mem, size_t sz) {
    kprintf("[MEM] Attaching 0x%lx - 0x%lx to kernel memory pool.\n", mem,
            mem + sz);
    if (sz <= sizeof(kmem_pool))
        return;
    kmem_pool *pool = (kmem_pool *)mem;
    memset(pool, 0, sizeof(kmem_pool));
    spinlock_init(&pool->lock);
    spinlock_acquire(&pool->lock);
    pool->size = sz - sizeof(kmem_pool);
    pool->free = pool->size;
    spinlock_acquire(&global_pool_lock);
    pool->next_pool = memory_pool;
    memory_pool     = pool;
    insert_into_pool(pool, mem + sizeof(kmem_pool), pool->free);
    spinlock_release(&global_pool_lock);
    spinlock_release(&pool->lock);
}

static void insert_into_pool(kmem_pool *pool, char *mem, size_t sz) {
    assert(pool, "Pool must be a valid pointer.");
    assert(sz >= sizeof(kmem_block), "Size must large than sizeof kmem_block.");

    kmem_block *block = (kmem_block *)mem;
    memset(block, 0, sizeof(kmem_block));
    rb_node *node = &block->tree_node;

    block->cookie = KMEM_COOKIE;
    block->type   = KMEM_TYPE_FREE;
    block->pool   = pool;
    block->size   = sz - kmem_block_head_size;
    node->key     = sz;

    rb_node *existed = rb_insert(&pool->free_tree, node);
    if (existed && existed != node) {
        /*
         * 当前存在同样大小的block，做如下操作：
         * 若已经存在的block有下一项，则插入下一项和已经存在的block之间
         * 若已经存在的block没有下一项，则该block为已经存在的block的下一项
         * 更新相应的type标志
         */
        kmem_block *eblock = container_of(existed, kmem_block, tree_node);
        assert(eblock->cookie == KMEM_COOKIE, "Existed Block invalid.");
        assert(eblock->type & KMEM_TYPE_IN_TREE,
               "Existed Block type mismatch.");
        assert(eblock->pool == pool, "Existed Block not inside our pool.");
        assert(existed->key == node->key, "Exitsed but not same key.");

        if (eblock->type & KMEM_TYPE_HAVE_NEXT) {
            assert(eblock->mem.list.next_free, "Have next but no next.");
            kmem_block *next = eblock->mem.list.next_free;
            assert(next->mem.list.prev_free == eblock, "list guard.");
            next->mem.list.prev_free   = block;
            block->mem.list.next_free  = next;
            eblock->mem.list.next_free = block;
            block->mem.list.prev_free  = eblock;
            // set flags
            block->type |= KMEM_TYPE_HAVE_NEXT;
        } else {
            eblock->mem.list.next_free = block;
            block->mem.list.prev_free  = eblock;
            eblock->type |= KMEM_TYPE_HAVE_NEXT;
        }
    } else {
        block->type |= KMEM_TYPE_IN_TREE;
        block->mem.list.next_free = NULL;
        block->mem.list.prev_free = NULL;
    }
}

// Trust block->tree_node is in tree
static void remove_from_pool(kmem_block *block) {
    assert(block->cookie == KMEM_COOKIE, "Kmem block invalid.");
    assert(block->pool, "Kmem Block have not inserted into a pool.");
    kmem_pool *pool = block->pool;
    /*
     * 将内存块从内存池中移除有如下步骤：
     * 判断内存块是否有树节点，如果有：
     *      如果有后继，则用后继替代
     *      如果没有后继，则删除树节点
     * 如果没有：
     *      如果有后继，将前驱和后继链接（因为块在池内，不在树上必然存在前驱）
     *      如果没有后继，将前驱设为无后继状态
     */

    kmem_block *prev = block->mem.list.prev_free;
    if (!(block->type & KMEM_TYPE_IN_TREE)) {
        assert(prev, "Not in tree but no prev");
        assert(prev->cookie == KMEM_COOKIE, "Prev block is not valid.");
        assert(prev->pool == pool, "Prev block is not inside correct pool.");
        assert(prev->mem.list.next_free == block,
               "Prev block's next is not current block.");
    } else {
        assert(prev == NULL, "In tree but have prev.");
    }

    kmem_block *next = block->mem.list.next_free;
    if (block->type & KMEM_TYPE_HAVE_NEXT) {
        assert(next, "Have next but no next.");
        assert(next->cookie == KMEM_COOKIE, "Next block is not valid.");
        assert(next->pool == pool, "Next block is not inside correct pool.");
        assert(next->mem.list.prev_free == block,
               "Next block's prev is not current block.");
    } else {
        assert(next == NULL, "Have no next but next not null.");
    }

    if (block->type & KMEM_TYPE_IN_TREE) {
        if (block->type & KMEM_TYPE_HAVE_NEXT) {
            rb_replace(&block->tree_node, &next->tree_node);
            next->mem.list.prev_free = NULL;
            next->type |= KMEM_TYPE_IN_TREE;
            block->type &= ~(KMEM_TYPE_HAVE_NEXT | KMEM_TYPE_IN_TREE);
        } else {
            rb_remove(&pool->free_tree, &block->tree_node);
            block->type &= ~(KMEM_TYPE_IN_TREE);
        }
    } else {
        if (block->type & KMEM_TYPE_HAVE_NEXT) {
            prev->mem.list.next_free = next;
            next->mem.list.prev_free = prev;
            block->type &= ~(KMEM_TYPE_HAVE_NEXT);
        } else {
            prev->mem.list.next_free = NULL;
            prev->type &= ~(KMEM_TYPE_HAVE_NEXT);
        }
    }
    block->mem.list.prev_free = block->mem.list.next_free = NULL;
}

static rb_node *rb_search_upper(rb_node *x, uint64_t key) {
    rb_node *closet = NULL;
    while (x != NULL) {
        if (x->key == key)
            break;
        if (x->key > key) {
            if ((!closet) || (closet->key > x->key))
                closet = x;
            x = x->L;
        } else
            x = x->R;
    }
    if (x)
        return x;
    else
        return closet;
}

char *kmalloc(size_t size) {
    /*
     * 申请内存操作如下：
     * 1. 遍历内存池，从池中找到一个能完全装下内存的最小块
     * 2. 将该块从内存池中取出
     * 3. 若块大小 - (头大小 + 所申请的大小)
     *    大于空闲内存块结构的大小，则将剩余的部分插入回内存池
     */
    spinlock_acquire(&kmem_lock);
    size = ROUNDUP_WITH(16, size);

    size_t need_size = size + kmem_block_head_size;
    if (need_size < sizeof(kmem_block)) {
        need_size = sizeof(kmem_block);
        size      = need_size - kmem_block_head_size;
    }
    assert(need_size >= sizeof(kmem_block), "Size guard.");
    // need_size 是所需的最小内存块大小

    kmem_pool *pool = memory_pool;
    rb_node   *node = NULL;
    spinlock_acquire(&pool->lock);
    while (pool && !node) {
        node = rb_search_upper(pool->free_tree.root, need_size);
        if (node == NULL) {
            if (need_size <= pool->free)
                kpanic("Kmem have free but cannot search tree.");
            spinlock_release(&pool->lock);
            pool = pool->next_pool;
            if (!pool)
                return NULL;
            spinlock_acquire(&pool->lock);
        }
    }
    if (node == NULL)
        return NULL;

    kmem_block *block = container_of(node, kmem_block, tree_node);
    remove_from_pool(block);
    size_t remaining_size =
        block->size - size; // 当前头这里用，所以不需要加头大小
    if (remaining_size > sizeof(kmem_block)) {
        char *new_block = block->mem.mem + size;
        block->size     = size;
        insert_into_pool(pool, new_block, remaining_size);
    }
    assert(pool->free_tree.root, "test.");
    block->type = KMEM_TYPE_INUSE;
    pool->free -= block->size + kmem_block_head_size;
    spinlock_release(&pool->lock);
    spinlock_release(&kmem_lock);
    return block->mem.mem;
}

void kfree(void *p) {
    spinlock_acquire(&kmem_lock);
    kmem_block *block = (kmem_block *)(p - kmem_block_head_size);
    assert(block->cookie == KMEM_COOKIE, "Memory block invalid at kfree.");
    kmem_block *next_block = (kmem_block *)(p + block->size);
    spinlock_t *pool_lock  = &block->pool->lock;
    spinlock_acquire(pool_lock);
    if ((char *)next_block < block->pool->mem + block->pool->size) {
        // Next block is inside pool
        assert(next_block->cookie == KMEM_COOKIE,
               "Next Block is not KMem block.");
        while (next_block->type & KMEM_TYPE_FREE) {
            // detach free block
            remove_from_pool(next_block);
            block->size += next_block->size + kmem_block_head_size;
            // search for next
            next_block = (kmem_block *)(next_block->mem.mem + next_block->size);
            if ((char *)next_block > block->pool->mem + block->pool->size) {
                break;
            }
        }
    }
    insert_into_pool(block->pool, (char *)block,
                     block->size + kmem_block_head_size);
    spinlock_release(pool_lock);
    spinlock_release(&kmem_lock);
}
