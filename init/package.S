.section .text.init_code
.align 16
.global _init_code_start_
.global _init_code_end_

_init_code_start:
    .incbin INIT_ELF
_init_code_end: