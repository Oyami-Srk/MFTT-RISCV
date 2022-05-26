//
// Created by shiroko on 22-5-26.
//

#ifndef __K210_MEMMAPS_H__
#define __K210_MEMMAPS_H__

#include <memory.h>

#define GPIOHS    0x38001000
#define DMAC      0x50000000
#define GPIO      0x50200000
#define SPI_SLAVE 0x50240000
#define FPIOA     0x502B0000
#define SPI0      0x52000000
#define SPI1      0x53000000
#define SPI2      0x54000000
#define SYSCTL    0x50440000

#define GPIOHS_V    (0x38001000 + HARDWARE_VBASE)
#define DMAC_V      (0x50000000 + HARDWARE_VBASE)
#define GPIO_V      (0x50200000 + HARDWARE_VBASE)
#define SPI_SLAVE_V (0x50240000 + HARDWARE_VBASE)
#define FPIOA_V     (0x502B0000 + HARDWARE_VBASE)
#define SPI0_V      (0x52000000 + HARDWARE_VBASE)
#define SPI1_V      (0x53000000 + HARDWARE_VBASE)
#define SPI2_V      (0x54000000 + HARDWARE_VBASE)
#define SYSCTL_V    (0x50440000 + HARDWARE_VBASE)

#endif // __K210_MEMMAPS_H__