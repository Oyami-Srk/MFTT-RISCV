#include "./libfdt.h"
#include <driver/console.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/fdt.h>

static int isprint(int c) { return (unsigned)c - 0x20 < 0x5f; }
static int isspace(int c) { return c == ' ' || (unsigned)c - '\t' < 5; }

#pragma clang diagnostic push
#pragma ide diagnostic   ignored "EndlessLoop"

/*
 * Heuristic to guess if this is a string or concatenated strings.
 * From UBoot
 */

static int is_printable_string(const void *data, int len) {
    const char *s = data;

    /* zero length is not */
    if (len == 0)
        return 0;

    /* must terminate with zero or '\n' */
    if (s[len - 1] != '\0' && s[len - 1] != '\n')
        return 0;

    /* printable or a null byte (concatenated strings) */
    while (((*s == '\0') || isprint(*s) || isspace(*s)) && (len > 0)) {
        /*
         * If we see a null, there are three possibilities:
         * 1) If len == 1, it is the end of the string, printable
         * 2) Next character also a null, not printable.
         * 3) Next character not a null, continue to check.
         */
        if (s[0] == '\0') {
            if (len == 1)
                return 1;
            if (s[1] == '\0')
                return 0;
        }
        s++;
        len--;
    }

    /* Not the null termination, or not done yet: not printable */
    if (*s != '\0' || (len != 0))
        return 0;

    return 1;
}

static int is_str_match_before_at(const char *dst, const char *src) {
    unsigned char *c1 = (unsigned char *)dst;
    unsigned char *c2 = (unsigned char *)src;
    while (1) {
        if (*c1 == '@' || *c2 == '@')
            break;
        if (*c1 != *c2)
            return *c1 < *c2 ? -1 : 1;
        if (!*c1)
            break;
        c1++;
        c2++;
    }
    return 0;
}

static prober_fp get_prober_fp(const char *node_name) {
    section_foreach_entry(FDTProbers, fdt_prober *, prober) {
        if (is_str_match_before_at((*prober)->name, node_name) == 0)
            return (*prober)->prober;
    }
    return NULL;
}

#pragma clang diagnostic pop

#define KVITEM(length, type) "%-" #length "s:\t" type "\n"

static void fdt_header_print(struct fdt_header *header) {
    uint32_t version = fdt32_to_cpu(header->version);
    kprintf("[FDT] ================ FDT Header ===============\n");
    kprintf(KVITEM(30, "%d"), "[FDT] Total size of DT block",
            fdt32_to_cpu(header->totalsize));
    kprintf(KVITEM(30, "%d"), "[FDT] Format Version", version);
    kprintf(KVITEM(30, "%d"), "[FDT] Last compatible version",
            fdt32_to_cpu(header->last_comp_version));
    if (version >= 2) {
        kprintf(KVITEM(30, "%d"), "[FDT] Boot CPU ID",
                fdt32_to_cpu(header->boot_cpuid_phys));
        if (version >= 3) {
            kprintf(KVITEM(30, "%d"), "[FDT] Size of strings block",
                    fdt32_to_cpu(header->size_dt_strings));
            if (version >= 17) {
                kprintf(KVITEM(30, "%d"), "[FDT] Size of struct block",
                        fdt32_to_cpu(header->size_dt_struct));
            }
        }
    }
    kprintf("[FDT] ============ End of FDT Header ============\n");
}

// return node end offset
// TODO: no-recursion
static int parse_node(uint32_t version, const char *begin, uint32_t addr_cells,
                      uint32_t size_cells, const char *strings) {
    const char *p = begin;
    uint32_t    tag;
    while ((tag = FDT_OFFSET_32(p, 0)) != FDT_END_NODE) {
        p += 4;
        switch (tag) {
        case FDT_BEGIN_NODE: {
            const char *str = p;
            p               = PALIGN(p + strlen(str) + 1, 4);
            assert(str != NULL, "[FDT] Node str cannot be null.");
            if (*str != '\0') {
                prober_fp fp = NULL;
                if ((fp = get_prober_fp(str)) != NULL) {
                    p += fp(version, str, p, addr_cells, size_cells, strings);
                } else {
                    p +=
                        parse_node(version, p, addr_cells, size_cells, strings);
                }
                assert(FDT_OFFSET_32(p, 0) == FDT_END_NODE,
                       "[FDT] Prober not give correct offset to its end.");
                p += 4;
            }
            break;
        }
        case FDT_NOP:
            break;
        case FDT_PROP: {
            uint32_t size = FDT_OFFSET_32(p, 0);
            p += 4;
            const char *str = strings + FDT_OFFSET_32(p, 0);
            p += 4;
            if (version < 16 && size >= 8)
                p = PALIGN(p, 8);
            const char *p_value = p;

            p = PALIGN(p + size, 4);

            if (strcmp("#address-cells", str) == 0) {
                assert(size == 4 || size == 8,
                       "[FDT] Only support cells with size 4 or 8.");
                if (size == 4)
                    addr_cells = FDT_OFFSET_32(p_value, 0);
                else
                    addr_cells = FDT_OFFSET_64(p_value, 0);
            } else if (strcmp("#size-cells", str) == 0) {
                assert(size == 4 || size == 8,
                       "[FDT] Only support cells with size 4 or 8.");
                if (size == 4)
                    size_cells = FDT_OFFSET_32(p_value, 0);
                else
                    size_cells = FDT_OFFSET_64(p_value, 0);
            }

            break;
        }
        default:
            kpanic("[FDT] Unknown FDT Tag Note: 0x%08x.\n", tag);
            break;
        }
    }
    return (int)(p - begin);
}

void init_fdt(struct fdt_header *header) {
    kprintf("[FDT] Starting decode Flatten Device Tree.\n");
    assert(FDT_MAGIC == fdt32_to_cpu(header->magic),
           "[FDT] Magic number incorrect.");
    uint32_t version = fdt32_to_cpu(header->version);
    fdt_header_print(header);
    // TODO: process reserve memory
    struct fdt_reserve_entry *rsvmap =
        (struct fdt_reserve_entry *)((uint8_t *)header +
                                     fdt32_ld(&header->off_mem_rsvmap));
    const char *strings =
        (const char *)((uint8_t *)header + fdt32_ld(&header->off_dt_strings));
    char *p = (char *)((uint8_t *)header + fdt32_ld(&header->off_dt_struct));

    uint32_t addr_cells = 0;
    uint32_t size_cells = 0;
    uint32_t tag;
    while ((tag = FDT_OFFSET_32(p, 0)) != FDT_END) {
        p += 4;
        switch (tag) {
        case FDT_BEGIN_NODE: {
            const char *str = p;
            p               = PALIGN(p + strlen(str) + 1, 4);
            if (*str != '\0') {
                prober_fp fp = NULL;
                if ((fp = get_prober_fp(str)) != NULL) {
                    p += fp(version, str, p, addr_cells, size_cells, strings);
                } else {
                    // TODO: non-rec skip sub-nodes
                    p +=
                        parse_node(version, p, addr_cells, size_cells, strings);
                }
                assert(FDT_OFFSET_32(p, 0) == FDT_END_NODE,
                       "[FDT] Prober not give correct offset to its end.");
                p += 4;
            }

            break;
        }
        case FDT_END_NODE: {
            break;
        }
        case FDT_NOP:
            break;
        case FDT_PROP: {
            uint32_t size = FDT_OFFSET_32(p, 0);
            p += 4;
            const char *str = strings + FDT_OFFSET_32(p, 0);
            p += 4;
            if (version < 16 && size >= 8)
                p = PALIGN(p, 8);
            const char *p_value = p;

            p = PALIGN(p + size, 4);
            // kprintf("// %s (size:%d) \n", str, size);
            // Check root properties
            if (strcmp("#address-cells", str) == 0) {
                assert(size == 4 || size == 8,
                       "[FDT] Only support cells with size 4 or 8.");
                if (size == 4)
                    addr_cells = FDT_OFFSET_32(p_value, 0);
                else
                    addr_cells = FDT_OFFSET_64(p_value, 0);
            } else if (strcmp("#size-cells", str) == 0) {
                assert(size == 4 || size == 8,
                       "[FDT] Only support cells with size 4 or 8.");
                if (size == 4)
                    size_cells = FDT_OFFSET_32(p_value, 0);
                else
                    size_cells = FDT_OFFSET_64(p_value, 0);
            } else if (strcmp("model", str) == 0) {
                kprintf("[FDT] Device model: %s\n", p_value);
            }
            break;
        }
        default:
            kpanic("[FDT] Unknown FDT Tag Note: 0x%08x.\n", tag);
            break;
        }
    }
}