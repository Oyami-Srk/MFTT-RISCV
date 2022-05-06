//
// Created by shiroko on 22-4-26.
//

#ifndef __PROC_H__
#define __PROC_H__

#include <common/types.h>
#include <lib/sys/spinlock.h>
#include <memory.h>

typedef uint32_t pid_t;

// All register
struct trap_context {
    uint64_t ra;
    uint64_t sp;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t s0;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
} __attribute__((packed));

// Only Callee saved ones
struct task_context {
    uint64_t ra; // 这个变量用来切换语境
    uint64_t sp;
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
};

#define PROC_STATUS_SUSPEND 0x0000
#define PROC_STATUS_RUNNING 0x0001
#define PROC_STATUS_READY   0x0002
#define PROC_STATUS_GOTINT  0x0004
#define PROC_STATUS_GOTEXC  0x0008
#define PROC_STATUS_NORMAL  0x0010
#define PROC_STATUS_STOP    0x0020
#define PROC_STATUS_ERROR   0x0040
#define PROC_STATUS_TRACING 0x0080
#define PROC_STATUS_WAITING 0x0100
#define PROC_STATUS_HANGING 0x0200
#define PROC_STATUS_DONE    0x0400

#define PROC_ANY       0xFFFFFFFE
#define PROC_INTERRUPT 0xFFFFFFFF

#define PROC_STACK_SIZE 8192
#define PROC_STACK_BASE 0x80000000

#define PROC_NAME_SIZE 16

struct __proc_t {
    /* 0 ~ 24 */
    pde_t    page_dir;
    void    *kernel_sp;
    void    *user_pc;
    uint64_t kernel_cpuid;
    /* 32 ~ 272 */
    struct trap_context trapframe;
    /* 280 ~ ... */

    // Actually Assembly doesn't need anything below
    pid_t               pid;
    uint32_t            status;
    struct __proc_t    *parent;
    void               *kernel_stack;
    struct task_context kernel_task_context;
    uint64_t            exit_status;
    spinlock_t          lock;
    char                name[PROC_NAME_SIZE];
    void               *waiting_chan;
    // Elf program infos
    size_t prog_size;
    char  *prog_break;       // in va
    char  *prog_image_start; // in va
    char  *prog_brk_pg_end;
} __attribute__((aligned(16)));

typedef struct __proc_t proc_t;

void init_proc();

proc_t *proc_alloc();
void    proc_free(proc_t *proc);
proc_t *myproc();

void sleep(void *chan, spinlock_t *lock);
void wakeup(void *chan);

/* Note:
 *
    32位指令opcode最低2位为“11”，而16位变长指令可以是“00、01、10”，48位指令低5位位全1，64位指令低6位全1。
 */
#endif // __PROC_H__