typedef unsigned long long uintptr_t;

static inline uintptr_t test_syscall(int val) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(val);
    register uintptr_t a7 asm("a7") = (uintptr_t)(123);
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

int main(void) {
    while (1) {
        for (unsigned long long i = 0; i < 2000000000; i++)
            asm volatile("nop");
        test_syscall(1);
    }
}
