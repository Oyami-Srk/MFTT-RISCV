/* Unlike x86, we do not need setup GDT or goto higher address
   Because of SBI. but I kinda feel uncool for that. */
.section .text.startup,"ax"
.global _start

_start:
    add t0, a0, 1    /* a0 = mhartid, set by SBI */
    slli t0, t0, 13  /* t0 = t0 << 13; */
    la sp, boot_sp
    add sp, sp, t0   /* Each CPU core got 8KB Boot stack */

    call kernel_main /* Direct jump into kernel
.1:
    j .1 /* loop for unreached code*/


.section .bss.stack
.global boot_stack
boot_stack:
    .space 1024 * 8 * 2 /* 16KB Boot stack */
.global boot_sp
boot_sp: