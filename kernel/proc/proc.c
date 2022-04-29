#include <proc.h>

_Static_assert(sizeof(struct trap_context) == sizeof(uint64_t) * 31,
               "Trap context wrong.");
