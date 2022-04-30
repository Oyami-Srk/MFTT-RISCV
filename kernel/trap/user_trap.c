//
// Created by shiroko on 22-4-30.
//

#include <common/types.h>
#include <driver/SBI.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <riscv.h>
#include <trap.h>

// Used in trap.S
void __attribute__((used)) user_trap_handler() {}
