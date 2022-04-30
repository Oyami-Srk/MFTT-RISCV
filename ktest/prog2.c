typedef unsigned long long uintptr_t;

uintptr_t test_syscall(int val) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(val);
    register uintptr_t a7 asm("a7") = (uintptr_t)(2);
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

int main(void) {
    while (1) {
        for (int i = 0; i < 100000; i++) {
            test_syscall(2);
            asm volatile("nop");
        }
    }
}
