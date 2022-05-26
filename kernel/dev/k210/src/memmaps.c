// MFTT-Driver Framework

#include "../memmaps.h"
#include <dev/dev.h>
#include <driver/console.h>
#include <memory.h>
#include <riscv.h>

int init_k210_memmaps(dev_driver_t *drv) {
    kprintf("[K210] K210 Peripherals' memory-maps start initialize.\n");
    // Map SYSCTL hardware addr
    mem_sysmap((void *)SYSCTL_V, (void *)SYSCTL, 0x1000, PTE_TYPE_RW);
    // Map FPIOA hardware addr
    mem_sysmap((void *)FPIOA_V, (void *)FPIOA, 0x1000, PTE_TYPE_RW);
    // Map GPIOHS hardware addr
    mem_sysmap((void *)GPIOHS_V, (void *)GPIOHS, 0x1000, PTE_TYPE_RW);
    // Map DMAC hardware addr
    mem_sysmap((void *)DMAC_V, (void *)DMAC, 0x1000, PTE_TYPE_RW);
    // Map SPI 0~2 hardware addr
    mem_sysmap((void *)SPI0_V, (void *)SPI0, 0x1000, PTE_TYPE_RW);
    mem_sysmap((void *)SPI1_V, (void *)SPI1, 0x1000, PTE_TYPE_RW);
    mem_sysmap((void *)SPI2_V, (void *)SPI2, 0x1000, PTE_TYPE_RW);
    // Map SPI Slave hardware addr
    mem_sysmap((void *)SPI_SLAVE_V, (void *)SPI_SLAVE, 0x1000, PTE_TYPE_RW);

    flush_tlb_all();
    kprintf("[K210] K210 Peripherals' memory-maps finished.\n");
    return 0;
}

dev_driver_t k210_memmap_driver = {
    .name = "k210-memmap",
    .init = init_k210_memmaps,
#ifdef PLATFORM_QEMU
    .loading_sequence = 0xFF,
#else
    .loading_sequence = 3,
#endif
    .dev_id       = 0,
    .major_ver    = 0,
    .minor_ver    = 1,
    .private_data = NULL,
    .list         = LIST_HEAD_INIT(k210_memmap_driver.list),
};

ADD_DEV_DRIVER(k210_memmap_driver);
