/* link script for qemu-riscv64 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS {
    _KERN_BASE = 0x80200000;
    . = _KERN_BASE;

    .text ALIGN(0x1000): {
        PROVIDE(__start_kernel_code = .);
        *(.text.startup)
        *(.text .stub .text.* .gnu.linkonce.t.*)
        PROVIDE(__end_kernel_code = .);
    }

    .rodata ALIGN(0x1000) : {
        *(.rodata .rodata.* .gnu.linkonce.r.* .init_code)
        . = ALIGN(0x10); /* not sure they should align to 8 bytes */
    }

    .stab ALIGN(0x1000) : { /* Debug information */
        *(.stab .stabstr)
        BYTE(0)
    }

    .data ALIGN(0x1000) : {
        *(.data .data.*)

        /* Driver's data */
        PROVIDE(__start_DevDrivers = .);
        *(DevDrivers)
        PROVIDE(__stop_DevDrivers = .);

        /* FDT Prober's data */
        PROVIDE(__start_FDTProbers = .);
        *(FDTProbers*)
        PROVIDE(__stop_FDTProbers = .);
    }

    .bss ALIGN(0x1000) : {
        PROVIDE(__bss_start = .);
        *(.bss.boot_stack)
        *(.bss.stack)
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(.ebss .ebss.*)
        PROVIDE(__bss_end = .);
    }

    _KERN_END = .;
}