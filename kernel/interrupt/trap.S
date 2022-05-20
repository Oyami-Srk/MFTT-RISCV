.section .text,"ax"
.global supervisor_interrupt_vector
.global user_interrupt_vector
.global user_ret
.align 4

supervisor_interrupt_vector:
    addi sp, sp, -256

    /* Save */
    sd ra, 0(sp)
    sd sp, 8(sp)
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd s0, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    mv a0, sp

    /* Call supervisor trap handler */
    call supervisor_trap_handler

    /* Restore */
    ld ra, 0(sp)
    ld sp, 8(sp)
    ld gp, 16(sp)
    ld tp, 24(sp)
    ld t0, 32(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)

    addi sp, sp, 256

    sret /* Return */

.align 4

user_interrupt_vector:
    /* 交换a0和sscratch，sscratch里面存放进程的结构体指针 */
    csrrw a0, sscratch, a0
    /* 保存上下文 */
    sd ra, 32(a0)
    sd sp, 40(a0)
    sd gp, 48(a0)
    sd tp, 56(a0)
    sd t0, 64(a0)
    sd t1, 72(a0)
    sd t2, 80(a0)
    sd s0, 88(a0)
    sd s1, 96(a0)
    /* sd a0, 104(a0) */
    sd a1, 112(a0)
    sd a2, 120(a0)
    sd a3, 128(a0)
    sd a4, 136(a0)
    sd a5, 144(a0)
    sd a6, 152(a0)
    sd a7, 160(a0)
    sd s2, 168(a0)
    sd s3, 176(a0)
    sd s4, 184(a0)
    sd s5, 192(a0)
    sd s6, 200(a0)
    sd s7, 208(a0)
    sd s8, 216(a0)
    sd s9, 224(a0)
    sd s10, 232(a0)
    sd s11, 240(a0)
    sd t3, 248(a0)
    sd t4, 256(a0)
    sd t5, 264(a0)
    sd t6, 272(a0)

    csrr t0, sscratch
    sd t0, 104(a0)

    csrr t0, sepc
    sd t0, 16(a0)

    ld sp, 8(a0)
    ld tp, 24(a0)

    /*  Since we just load all kernel space into user page dir
        There is no need for switching page dir */

    j user_trap_handler

user_ret:
    /* a0: 进程结构体指针 */
    /* Switch page table */
    ld t0, 0(a0)
    /* satp = (page_dir >> 12) | PAGING_MODE_SV39 << 60 */
    /*
    srli t0, t0, 12
    li   t1, 8
    slli t1, t1, 60
    or   t0, t0, t1 */
    csrw satp, t0
    sfence.vma

    ld t0, 104(a0)
    csrw sscratch, t0

    /* 恢复上下文 */
    ld ra, 32(a0)
    ld sp, 40(a0)
    ld gp, 48(a0)
    ld tp, 56(a0)
    ld t0, 64(a0)
    ld t1, 72(a0)
    ld t2, 80(a0)
    ld s0, 88(a0)
    ld s1, 96(a0)
    /* ld a0, 104(a0) */
    ld a1, 112(a0)
    ld a2, 120(a0)
    ld a3, 128(a0)
    ld a4, 136(a0)
    ld a5, 144(a0)
    ld a6, 152(a0)
    ld a7, 160(a0)
    ld s2, 168(a0)
    ld s3, 176(a0)
    ld s4, 184(a0)
    ld s5, 192(a0)
    ld s6, 200(a0)
    ld s7, 208(a0)
    ld s8, 216(a0)
    ld s9, 224(a0)
    ld s10, 232(a0)
    ld s11, 240(a0)
    ld t3, 248(a0)
    ld t4, 256(a0)
    ld t5, 264(a0)
    ld t6, 272(a0)

    csrrw a0, sscratch, a0

    sret
