/* Unlike x86, we do not need setup GDT or goto higher address
   Because of SBI. but I kinda feel uncool for that. */
.section .text.startup,"ax"
.global _start

_start:
    /* Mask all interrupts */
    csrw sie, 0

    add t0, a0, 1    /* a0 = mhartid, set by SBI */
    // slli t0, t0, 14  /* t0 = t0 << 14; */
    slli t0, t0, 16  /* t0 = t0 << 14; */
    la sp, boot_stack
    add sp, sp, t0   /* Each CPU core got 4KB Boot stack */

    call kernel_main /* Direct jump into kernel
.1:
    j .1 /* loop for unreached code*/


.section .bss.boot_stack
.global boot_stack
boot_stack:
    // .space 1024 * 16 * 2 /* 4KB kernel boot stack For ecah core*/
    .space 1024 * 64 * 2 /* 4KB kernel boot stack For ecah core*/
.global boot_sp
boot_sp: