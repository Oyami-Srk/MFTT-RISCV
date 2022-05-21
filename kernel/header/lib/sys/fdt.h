// Code from Linux libfdt

#ifndef __FDT_H__
#define __FDT_H__

#include <types.h>

typedef uint32_t fdt32_t;
typedef uint64_t fdt64_t;

struct fdt_header {
    fdt32_t magic;             /* magic word FDT_MAGIC */
    fdt32_t totalsize;         /* total size of DT block */
    fdt32_t off_dt_struct;     /* offset to structure */
    fdt32_t off_dt_strings;    /* offset to strings */
    fdt32_t off_mem_rsvmap;    /* offset to memory reserve map */
    fdt32_t version;           /* format version */
    fdt32_t last_comp_version; /* last compatible version */

    /* version 2 fields below */
    fdt32_t boot_cpuid_phys; /* Which physical CPU id we're
                            booting on */
    /* version 3 fields below */
    fdt32_t size_dt_strings; /* size of the strings block */

    /* version 17 fields below */
    fdt32_t size_dt_struct; /* size of the structure block */
};

struct fdt_reserve_entry {
    fdt64_t address;
    fdt64_t size;
};

struct fdt_node_header {
    fdt32_t tag;
    char    name[0];
};

struct fdt_property {
    fdt32_t tag;
    fdt32_t len;
    fdt32_t nameoff;
    char    data[0];
};

// clang-format off
#define FDT_MAGIC	0xd00dfeed	/* 4: version, 4: total size */
#define FDT_TAGSIZE	sizeof(fdt32_t)

#define FDT_BEGIN_NODE	0x1		/* Start node: full name */
#define FDT_END_NODE	0x2		/* End node */
#define FDT_PROP	0x3		/* Property: name off,
					   size, content */
#define FDT_NOP		0x4		/* nop */
#define FDT_END		0x9

#define FDT_V1_SIZE	(7*sizeof(fdt32_t))
#define FDT_V2_SIZE	(FDT_V1_SIZE + sizeof(fdt32_t))
#define FDT_V3_SIZE	(FDT_V2_SIZE + sizeof(fdt32_t))
#define FDT_V16_SIZE	FDT_V3_SIZE
#define FDT_V17_SIZE	(FDT_V16_SIZE + sizeof(fdt32_t))

// clang-format on

typedef int (*prober_fp)(uint32_t version, const char *begin,
                         const char *node_name, uint32_t addr_cells,
                         uint32_t size_cells, const char *strings);
typedef struct __fdt_prober_t {
    const char *name; // uniq, todo: may should build a trie tree?
    prober_fp   prober;
} fdt_prober;

#define ADD_FDT_PROBER(prober)                                                 \
    static fdt_prober *__ptr##prober                                           \
        __attribute__((used, section("FDTProbers"))) = &prober

static inline uint32_t fdt32_ld(const fdt32_t *p) {
    const uint8_t *bp = (const uint8_t *)p;

    return ((uint32_t)bp[0] << 24) | ((uint32_t)bp[1] << 16) |
           ((uint32_t)bp[2] << 8) | bp[3];
}

static inline uint64_t fdt64_ld(const fdt64_t *p) {
    const uint8_t *bp = (const uint8_t *)p;

    return ((uint64_t)bp[0] << 56) | ((uint64_t)bp[1] << 48) |
           ((uint64_t)bp[2] << 40) | ((uint64_t)bp[3] << 32) |
           ((uint64_t)bp[4] << 24) | ((uint64_t)bp[5] << 16) |
           ((uint64_t)bp[6] << 8) | bp[7];
}

#define EXTRACT_BYTE(x, n) ((unsigned long long)((uint8_t *)&(x))[n])
#define CPU_TO_FDT32(x)                                                        \
    ((EXTRACT_BYTE(x, 0) << 24) | (EXTRACT_BYTE(x, 1) << 16) |                 \
     (EXTRACT_BYTE(x, 2) << 8) | EXTRACT_BYTE(x, 3))
#define CPU_TO_FDT64(x)                                                        \
    ((EXTRACT_BYTE(x, 0) << 56) | (EXTRACT_BYTE(x, 1) << 48) |                 \
     (EXTRACT_BYTE(x, 2) << 40) | (EXTRACT_BYTE(x, 3) << 32) |                 \
     (EXTRACT_BYTE(x, 4) << 24) | (EXTRACT_BYTE(x, 5) << 16) |                 \
     (EXTRACT_BYTE(x, 6) << 8) | EXTRACT_BYTE(x, 7))
#define FDT_OFFSET(node, offset) (((char *)(node)) + (offset))
#define FDT_OFFSET_32(node, offset)                                            \
    ((uint32_t)CPU_TO_FDT32((*((const fdt32_t *)FDT_OFFSET(node, offset)))))
#define FDT_OFFSET_64(node, offset)                                            \
    ((uint64_t)CPU_TO_FDT64((*((const fdt64_t *)FDT_OFFSET(node, offset)))))
#define ALIGN(x, a)  (((x) + ((a)-1)) & ~((a)-1))
#define PALIGN(p, a) ((void *)(ALIGN((unsigned long)(p), (a))))

// Wow, wonderful, C macros, awesome!

static inline uint32_t fdt32_to_cpu(fdt32_t x) {
    return (uint32_t)CPU_TO_FDT32(x);
}

static inline uint64_t fdt64_to_cpu(fdt64_t x) {
    return (uint64_t)CPU_TO_FDT64(x);
}

void init_fdt(struct fdt_header *header);

#endif // __FDT_H__