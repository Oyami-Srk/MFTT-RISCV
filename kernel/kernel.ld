/* link script for qemu-riscv64 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS {
    _KERN_BASE = 0x80200000;
    . = _KERN_BASE;

    .text ALIGN(0x1000): {
        *(.text.startup)
        . = ALIGN(4K);
        *(.text .stub .text.* .gnu.linkonce.t.*)
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
    }

    .bss ALIGN(0x1000) : {
        *(.bss.stack)
        *(.bss .bss.*)
        *(.sbss .sbss.*)
    }

    _KERN_END = .;
}