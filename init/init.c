//
// Created by shiroko on 22-5-6.
//

#include <stdlib.h>
#include <syscall.h>

int main() {
    print("Hello world.\n");
    char buf[32];
    for (;;) {
        itoa(ticks(), buf, 10);
        print(buf);
        print("\n");
        sleep(2);
    }
}