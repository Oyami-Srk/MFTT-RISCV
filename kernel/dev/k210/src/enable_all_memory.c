//
// Created by shiroko on 22-6-4.
//

#include "../memmaps.h"
#include "../sysctl.h"
#include <driver/console.h>

volatile sysctl_t *const sysctl_pa = (volatile sysctl_t *)SYSCTL;

void enable_all_memory() {
    kprintf("[K210] Try Enable PLL1.\n");

    sysctl_pa->pll1.pll_bypass1 = 0;
    sysctl_pa->pll1.pll_pwrd1   = 1;
    sysctl_pa->pll1.pll_reset1  = 0;
    sysctl_pa->pll1.pll_reset1  = 1;
    asm volatile("nop");
    asm volatile("nop");
    sysctl_pa->pll1.pll_reset1 = 0;

    kprintf("[K210] Try Open PLL1's clock.\n");

    sysctl_pa->pll1.pll_out_en1 = 1;

    kprintf("[K210] Finished.\n");
}