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
    size_t              size; // *not include* pool header
    struct __kmem_pool *next_pool;
    size_t              free;
    rb_tree             free_tree;
    spinlock_t          lock;
    char                mem[0];
};

typedef struct __kmem_pool kmem_pool;
static spinlock_t          kmem_lock = {.lock = false, .cpu = 0};

#define KMEM_MAKE_CLEAN 1
#define KMEM_COOKIE     0xD1AC

#define KMEM_TYPE_FREE      0x0001
#define KMEM_TYPE_INUSE     0x1000
#define KMEM_TYPE_HAVE_NEXT 0x0002
#define KMEM_TYPE_IN_TREE   0x0004

//#define KMEM_BEGIN_CANARY 0xBEC5C5C5BABABABAL
//#define KMEM_END_CANARY   0xED9D9D9D5A5A5A5AL
#define KMEM_BEGIN_CANARY 0xBEBEBEBEBEBEBEBEL
#define KMEM_END_CANARY   0xEDEDEDEDEDEDEDEDL

// TODO: Smaller the block, glibc is amazing at pool management
struct __kmem_block {
    uint16_t cookie;
    uint16_t type;
    // We assume there is nothing need larger than 2^32-1 bytes.
    uint32_t   size; // size is memory block size, **include** header.
    kmem_pool *pool;
    union __mem {
        char mem[0];
        struct __kmem_block_free {
            struct __kmem_block *prev_free;
            struct __kmem_block *next_free;
            rb_node              node; // node->key = size
        } free;                        // when free
    } mem;
} __attribute__((aligned(4)));

typedef struct __kmem_block kmem_block;

static const size_t kmem_block_head_size =
    sizeof(uint16_t) * 2 + sizeof(uint32_t) + sizeof(kmem_pool *);

static const size_t minimal_block_size = ROUNDUP_WITH(16, sizeof(kmem_block));

static const size_t canary_size = sizeof(uint64_t);

_Static_assert(ROUNDUP_WITH(16, sizeof(kmem_block)) == 64,
               "size assert failed.");

_Static_assert(sizeof(kmem_block) == sizeof(uint16_t) * 2 + sizeof(uint32_t) +
                                         sizeof(kmem_pool *) +
                                         sizeof(struct __kmem_block_free),
               "Kmem block size assert failed.");

// rbtree->key is size, include struct itself

static kmem_pool *memory_pool      = NULL;
static spinlock_t global_pool_lock = {.lock = 0, .cpu = 0};

static void insert_into_pool(kmem_pool *pool, char *mem, size_t sz);
static void remove_from_pool(kmem_block *block);

static size_t get_tree_totalsize(rb_node *node) {
    if (!node)
        return 0;
    rb_node    *l     = node->L;
    rb_node    *r     = node->R;
    size_t      sz    = node->key;
    kmem_block *block = container_of(node, kmem_block, mem.free.node);
    while (block->type & KMEM_TYPE_HAVE_NEXT) {
        block = block->mem.free.next_free;
        sz += block->size;
        if (block->size != node->key)
            kpanic("Listed kmem block not same size.");
    }
    if (l)
        sz += get_tree_totalsize(l);
    if (r)
        sz += get_tree_totalsize(r);
    return sz;
}

void attach_to_memory_pool(char *mem, size_t sz) {
    kprintf("[MEM] Attaching 0x%lx - 0x%lx to kernel memory pool.\n", mem,
            mem + sz);
    if (((uintptr_t)mem & 0xF) != 0)
        return; // must aligned
    if (sz <= sizeof(kmem_pool))
        return;

    kmem_pool *pool = (kmem_pool *)mem;
    memset(pool, 0, sizeof(kmem_pool));
    spinlock_init(&pool->lock);
    spinlock_acquire(&pool->lock);
    pool->size = sz - sizeof(kmem_pool); // 减去内存池头的大小
    pool->free = pool->size;
    assert(mem + sz == pool->mem + pool->free, "Pool size Not match");
    spinlock_acquire(&global_pool_lock);
    pool->next_pool = memory_pool;
    memory_pool     = pool;
    insert_into_pool(pool, mem + sizeof(kmem_pool), pool->free);
    spinlock_release(&global_pool_lock);
    spinlock_release(&pool->lock);
}

// 向内存池中插入一块地址为 mem 大小为 sz 的块内存
static void insert_into_pool(kmem_pool *pool, char *mem, size_t sz) {
    assert(pool, "Pool must be a valid pointer.");
    assert(mem, "Mem must be a valid pointer (Null protector).");
    assert(sz >= minimal_block_size,
           "Mem size must be larger than minimal block size");

    kmem_block *block = (kmem_block *)mem;
    memset(block, 0, sizeof(kmem_block));

    rb_node *node = &block->mem.free.node;

    block->cookie = KMEM_COOKIE;
    block->type   = KMEM_TYPE_FREE;
    block->pool   = pool;
    block->size = node->key = sz;

    rb_node *existed = rb_insert(&pool->free_tree, node);
    if (existed && existed != node) {
        /*
         * 当前存在同样大小的block，做如下操作：
         * 若已经存在的block有下一项，则插入下一项和已经存在的block之间
         * 若已经存在的block没有下一项，则该block为已经存在的block的下一项
         * 更新相应的type标志
         */
        kmem_block *eblock = container_of(existed, kmem_block, mem.free.node);

        assert(eblock->cookie == KMEM_COOKIE, "Existed Block invalid.");
        assert(eblock->type & KMEM_TYPE_IN_TREE,
               "Existed Block type mismatch.");
        assert(eblock->pool == pool, "Existed Block not inside our pool.");
        assert(existed->key == node->key, "Exitsed but not same key.");

        if (eblock->type & KMEM_TYPE_HAVE_NEXT) {
            assert(eblock->mem.free.next_free, "Have next but no next.");
            kmem_block *next = eblock->mem.free.next_free;
            assert(next->mem.free.prev_free == eblock, "list guard.");
            next->mem.free.prev_free   = block;
            block->mem.free.next_free  = next;
            eblock->mem.free.next_free = block;
            block->mem.free.prev_free  = eblock;
            // set flags
            block->type |= KMEM_TYPE_HAVE_NEXT;
        } else {
            eblock->mem.free.next_free = block;
            block->mem.free.prev_free  = eblock;
            eblock->type |= KMEM_TYPE_HAVE_NEXT;
        }
    } else {
        block->type |= KMEM_TYPE_IN_TREE;
        block->mem.free.next_free = NULL;
        block->mem.free.prev_free = NULL;
    }
}

// Trust block->tree_node is in tree
static void remove_from_pool(kmem_block *block) {
    assert(block->cookie == KMEM_COOKIE, "Kmem block invalid.");
    assert(block->pool, "Kmem Block have not inserted into a pool.");
    kmem_pool *pool = block->pool;
    rb_node   *node = &block->mem.free.node;

    /*
     * 将内存块从内存池中移除有如下步骤：
     * 判断内存块是否有树节点，如果有：
     *      如果有后继，则用后继替代
     *      如果没有后继，则删除树节点
     * 如果没有：
     *      如果有后继，将前驱和后继链接（因为块在池内，不在树上必然存在前驱）
     *      如果没有后继，将前驱设为无后继状态
     */

    kmem_block *prev = block->mem.free.prev_free;
    if (!(block->type & KMEM_TYPE_IN_TREE)) {
        assert(prev, "Not in tree but no prev");
        assert(prev->cookie == KMEM_COOKIE, "Prev block is not valid.");
        assert(prev->pool == pool, "Prev block is not inside correct pool.");
        assert(prev->mem.free.next_free == block,
               "Prev block's next is not current block.");
    } else {
        assert(prev == NULL, "In tree but have prev.");
    }

    kmem_block *next = block->mem.free.next_free;
    if (block->type & KMEM_TYPE_HAVE_NEXT) {
        assert(next, "Have next but no next.");
        assert(next->cookie == KMEM_COOKIE, "Next block is not valid.");
        assert(next->pool == pool, "Next block is not inside correct pool.");
        assert(next->mem.free.prev_free == block,
               "Next block's prev is not current block.");
    } else {
        assert(next == NULL, "Have no next but next not null.");
    }

    if (block->type & KMEM_TYPE_IN_TREE) {
        assert(node->key, "In tree but key is 0.");
        if (block->type & KMEM_TYPE_HAVE_NEXT) {
            rb_replace(&pool->free_tree, node, &next->mem.free.node);
            next->mem.free.prev_free = NULL;
            next->type |= KMEM_TYPE_IN_TREE;
            block->type &= ~(KMEM_TYPE_HAVE_NEXT | KMEM_TYPE_IN_TREE);
        } else {
            rb_remove(&pool->free_tree, node);
            block->type &= ~(KMEM_TYPE_IN_TREE);
        }
    } else {
        if (block->type & KMEM_TYPE_HAVE_NEXT) {
            prev->mem.free.next_free = next;
            next->mem.free.prev_free = prev;
            block->type &= ~(KMEM_TYPE_HAVE_NEXT);
        } else {
            prev->mem.free.next_free = NULL;
            prev->type &= ~(KMEM_TYPE_HAVE_NEXT);
        }
    }
    block->mem.free.prev_free = block->mem.free.next_free = NULL;
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

/*
 * kmem提供kmalloc和kfree两个函数作为内核内小块内存分配器。
 * TODO: Slab内存分配器
 */

char *kmalloc(size_t size) {
    /*
     * 申请内存操作如下：
     * 1. 遍历内存池，从池中找到一个能完全装下内存的最小块
     * 2. 将该块从内存池中取出
     * 3. 若块大小 - (头大小 + 所申请的大小)
     *    大于空闲内存块结构的大小，则将剩余的部分插入回内存池
     * 4. 返回前设置金丝雀值
     */
    spinlock_acquire(&kmem_lock);
    size += canary_size * 2; // add canary
    size = ROUNDUP_WITH(16, size);

    size_t need_size = size + kmem_block_head_size;
    if (need_size < minimal_block_size) {
        need_size = minimal_block_size;
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

    rb_validate(pool->free_tree.root);
    kmem_block *block = container_of(node, kmem_block, mem.free.node);
    remove_from_pool(block);
    size_t remaining_size = block->size - need_size;
    if (remaining_size >= minimal_block_size) {
        block->size     = need_size;
        char *new_block = block->mem.mem + size;
        assert(((char *)block) + need_size == new_block, "Size not match.");
        insert_into_pool(pool, new_block, remaining_size);
    } else {
        size = block->size - kmem_block_head_size;
    }
    block->type = KMEM_TYPE_INUSE;
    pool->free -= block->size;
    rb_validate(pool->free_tree.root);
    size_t total_size = get_tree_totalsize(pool->free_tree.root);
    assert(pool->free == total_size, "Size not match.");
    spinlock_release(&pool->lock);
    spinlock_release(&kmem_lock);

    // set canary
    uint64_t *begin_canary = (uint64_t *)block->mem.mem;
    uint64_t *end_canary   = (uint64_t *)(block->mem.mem + size - canary_size);

    *begin_canary = KMEM_BEGIN_CANARY;
    *end_canary   = KMEM_END_CANARY;
#if KMEM_MAKE_CLEAN
    memset(((char *)begin_canary) + canary_size, 0,
           (uintptr_t)end_canary - (uintptr_t)begin_canary - 2 * canary_size);
#endif

    return block->mem.mem + canary_size;
}

void kfree(void *p) {
    spinlock_acquire(&kmem_lock);
    kmem_block *block = (kmem_block *)(p - kmem_block_head_size - canary_size);
    assert(block->cookie == KMEM_COOKIE, "Memory block invalid at kfree.");
    assert(block->type == KMEM_TYPE_INUSE, "Memory block not inuse.");
    uint64_t *begin_canary = (uint64_t *)block->mem.mem;
    uint64_t *end_canary =
        (uint64_t *)(((char *)block) + block->size - canary_size);
    assert(*begin_canary == KMEM_BEGIN_CANARY, "Begin canary dead.");
    assert(*end_canary == KMEM_END_CANARY, "End canary dead.");

#if KMEM_MAKE_CLEAN
    memset(begin_canary, 0xFA,
           (uintptr_t)end_canary - (uintptr_t)begin_canary + canary_size);
#endif

    kmem_pool *pool = block->pool;

    size_t total_size = get_tree_totalsize(pool->free_tree.root);
    assert(pool->free == total_size, "Size not match.");

    kmem_block *next_block = (kmem_block *)((char *)block + block->size);
    spinlock_acquire(&pool->lock);
    rb_validate(pool->free_tree.root);
    if ((char *)next_block < pool->mem + pool->size) {
        // Next block is inside pool
        assert(next_block->cookie == KMEM_COOKIE,
               "Next Block is not KMem block.");
        while (next_block->type & KMEM_TYPE_FREE) {
            assert(next_block->pool == pool, "Next block pool not match.");
            // detach free block
            remove_from_pool(next_block);
            pool->free -= next_block->size;
            block->size += next_block->size;
            // search for next
            char *m    = (char *)next_block;
            next_block = (kmem_block *)(m + next_block->size);
            // clear next_block head
            memset(m, 0xBA, sizeof(kmem_block));
            if ((char *)next_block >= pool->mem + pool->size) {
                break;
            }
        }
    }

    insert_into_pool(pool, (char *)block, block->size);

    pool->free += block->size;
    rb_validate(pool->free_tree.root);
    total_size = get_tree_totalsize(pool->free_tree.root);
    assert(pool->free == total_size, "Size not match.");
    spinlock_release(&pool->lock);
    spinlock_release(&kmem_lock);
}
