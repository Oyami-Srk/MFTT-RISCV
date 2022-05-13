//
// Created by shiroko on 22-5-6.
//

#include <stdlib.h>
#include <syscall.h>

int main() {
    print("Hello world.\n");
    char buf[32];
    for (;;) {
        for (unsigned long long i = 0; i < 200000000; i++)
            asm volatile("nop");
        itoa(ticks(), buf, 10);
        //        itoa(123, buf, 10);
        print(buf);
        print("\n");
    }
}