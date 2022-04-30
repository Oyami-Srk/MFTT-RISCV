#warn __BIN_PATH__
.section .prog_code
.align 16
.global _prog1_bin_start_
.global _prog1_bin_end_
.global _prog2_bin_start_
.global _prog2_bin_end_
.global _prog3_bin_start_
.global _prog3_bin_end_

_prog1_bin_start_:
    .incbin PROG1
_prog1_bin_end_:
_prog2_bin_start_:
    .incbin PROG2
_prog2_bin_end_:
_prog3_bin_start_:
    .incbin PROG3
_prog3_bin_end_:
