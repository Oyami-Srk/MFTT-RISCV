//
// Created by shiroko on 22-5-19.
//

#include "./plic.h"
#include <configs.h>
#include <driver/console.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <memory.h>
#include <riscv.h>
#include <types.h>

static char  *plic_pa       = 0;
static size_t plic_mem_size = 0;

void init_plic() {
    assert(plic_pa != 0, "PLIC Controller not found.");
    mem_sysmap((char *)PLIC_VA, plic_pa, plic_mem_size, PTE_TYPE_RW);
    flush_tlb_all();

#if !USE_SOFT_INT_COMP
    // set each hart's priority for S-mode
    for (int hart = 0; hart < HART_COUNT; hart++) {
        int context;
        context = hart * 2 + 1;
        MEM_IO_WRITE(uint32_t,
                     PLIC_MISC_ADDR_FOR(context) + PLIC_MISC_PRIO_THRESHOLD, 0);
    }
#endif
}

int plic_begin() {
    // Claim the interrupt and return irq
    int context =
#if USE_SOFT_INT_COMP
        // Machine context offset is 0
        cpuid() * 2 + 0;
#else
        // Supervisor context offset is 1
        cpuid() * 2 + 1; // NOLINT(cppcoreguidelines-narrowing-conversions)
#endif
    return MEM_IO_READ(uint32_t,
                       PLIC_MISC_ADDR_FOR(context) + PLIC_MISC_CLAIM_COMPLETE);
}

void plic_end(int irq) {
    // Complete the interrupt
    int context =
#if USE_SOFT_INT_COMP
        // Machine context offset is 0
        (int)cpuid() * 2 + 0;
#else
        // Supervisor context offset is 1
        (int)cpuid() * 2 + 1;
#endif
    MEM_IO_WRITE(uint32_t,
                 PLIC_MISC_ADDR_FOR(context) + PLIC_MISC_CLAIM_COMPLETE, irq);
}

int plic_register_irq(int irq) {
    kprintf("[PLIC] Register IRQ %d.\n", irq);
    // 1 is prio
    MEM_IO_WRITE(uint32_t, PLIC_VA + irq * sizeof(uint32_t), 1);
    // enable for each hart
    for (int hart = 0; hart < HART_COUNT; hart++) {
        int context;
#if USE_SOFT_INT_COMP
        // Machine context offset is 0
        context = hart * 2 + 0;
#else
        // Supervisor context offset is 1
        context = hart * 2 + 1;
#endif
        PLIC_ENABLE_INT_FOR(context, irq);
    }
    return 0;
}

// PLIC FDT Prober
#include <lib/sys/fdt.h>
static int plic_fdt_prober(uint32_t version, const char *node_name,
                           const char *begin, uint32_t addr_cells,
                           uint32_t size_cells, const char *strings) {
    kprintf("[FDT] FDT Prober for PLIC with node name: %s\n", node_name);
    const char *p = begin;
    uint32_t    tag;
    int         depth     = 1;
    int         discoverd = 0;
    while ((tag = FDT_OFFSET_32(p, 0)) != FDT_END_NODE && depth != 0) {
        p += 4;
        switch (tag) {
        case FDT_BEGIN_NODE: {
            const char *str = p;
            p               = PALIGN(p + strlen(str) + 1, 4);
            assert(str != NULL, "Node str cannot be null.");
            depth++;
            break;
        }
        case FDT_END_NODE:
            depth--;
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
            if (strcmp(str, "reg") == 0) {
                uint64_t plic_addr = 0x0;
                plic_mem_size      = 0x0;
                fdt32_t *addr_base =
                    (fdt32_t *)(p_value + sizeof(fdt32_t) * (addr_cells - 1));
                fdt32_t *size_base =
                    (fdt32_t *)(p_value + sizeof(fdt32_t) *
                                              (addr_cells + size_cells - 1));
                plic_addr = CPU_TO_FDT32(*addr_base);
                if (addr_cells >= 2)
                    plic_addr |= CPU_TO_FDT32(*(addr_base - 1)) << 32;
                plic_mem_size = CPU_TO_FDT32(*size_base);
                if (size_cells >= 2)
                    plic_mem_size |= CPU_TO_FDT32(*(size_base - 1)) << 32;
                kprintf("[FDT] Detected PLIC @ 0x%x with Size 0x%x bytes.\n",
                        plic_addr, plic_mem_size);
                discoverd = TRUE;
                plic_pa   = (char *)plic_addr;
            }
            break;
        }
        default:
            kpanic("Unknown FDT Tag Note: 0x%08x.\n", tag);
            break;
        }
    }
    if (!discoverd)
        kprintf("[FDT] Current round not detected PLIC info.\n");
    return (int)(p - begin);
}

static fdt_prober prober = {.name = "plic", .prober = plic_fdt_prober};

// for K210 TODO: use generic 'or'
static fdt_prober prober2 = {.name   = "interrupt-controller",
                             .prober = plic_fdt_prober};

ADD_FDT_PROBER(prober);
ADD_FDT_PROBER(prober2);
