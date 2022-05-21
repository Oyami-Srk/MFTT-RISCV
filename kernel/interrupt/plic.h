//
// Created by shiroko on 22-5-19.
//

#ifndef __INTERRUPT_PLIC_H__
#define __INTERRUPT_PLIC_H__

#include <types.h>
#include <memory.h>

#define PLIC_BASE_OFFSET            0x0
#define PLIC_INTPRIO_OFFSET         0x0
#define PLIC_INTENBITS_OFFSET       0x2000
#define PLIC_INTENBITS_CONTEXT_SIZE 0x80
#define PLIC_INTENBITS_SIZE         (sizeof(uint32_t))
#define PLIC_MISC_CONTEXT_OFFSET    0x200000
#define PLIC_MISC_CONTEXT_SIZE      0x1000
#define PLIC_MISC_PRIO_THRESHOLD    0x0
#define PLIC_MISC_CLAIM_COMPLETE    0x4

#define PLIC_INT_ENABLE_ADDR_FOR(context, irq)                                 \
    (HARDWARE_VBASE + PLIC_BASE_OFFSET + PLIC_INTENBITS_OFFSET +               \
     context * PLIC_INTENBITS_CONTEXT_SIZE + (irq / PLIC_INTENBITS_SIZE))
#define PLIC_ENABLE_INT_FOR(context, irq)                                      \
    MEM_IO_WRITE(                                                              \
        uint32_t, PLIC_INT_ENABLE_ADDR_FOR(context, irq),                      \
        MEM_IO_READ(uint32_t, PLIC_INT_ENABLE_ADDR_FOR(context, irq)) |        \
            (1 << (irq % PLIC_INTENBITS_SIZE)))
#define PLIC_MISC_ADDR_FOR(context)                                            \
    (HARDWARE_VBASE + PLIC_BASE_OFFSET + PLIC_MISC_CONTEXT_OFFSET +            \
     context * PLIC_MISC_CONTEXT_SIZE)

int  plic_begin(); // return irq
void plic_end(int irq);

#endif // __INTERRUPT_PLIC_H__