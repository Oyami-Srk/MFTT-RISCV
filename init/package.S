.section .text.init_code
.align 16
.global _init_code_start
.global _init_code_end

_init_code_start:
    .incbin INIT_ELF
_init_code_end:
