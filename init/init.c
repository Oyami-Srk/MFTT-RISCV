//
// Created by shiroko on 22-5-6.
//

#include <syscall.h>

int main() {
    print("Hello world.!");
    for (;;) {
        for (unsigned long long i = 0; i < 2000000000; i++)
            asm volatile("nop");
    }
}