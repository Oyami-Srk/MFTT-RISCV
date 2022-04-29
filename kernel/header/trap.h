//
// Created by shiroko on 22-4-26.
//

#ifndef __TRAP_H__
#define __TRAP_H__

#define TRAP_MODE_DIRECT   0x00
#define TRAP_MODE_VECTORED 0x01
#define TRAP_MODE          TRAP_MODE_DIRECT

void init_trap();

void trap_push_off();
void trap_pop_off();

#endif // __TRAP_H__