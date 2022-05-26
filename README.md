# MFTT-RISCV

Original project host on github: https://github.com/Oyami-Srk/MFTT-RISCV

My First Touch To RISCV

*WHAT A PITY THAT OJ HAVE NO SUPPORT FOR CMAKE*

What I've done:

* Boot
* Memory allocator(Page allocator based on buddy system and memory pool based on RB-Tree)
* Trap handler
* Process managment
* Copy-on-write fork
* Virtual File System
* General FDT Prober framework
* General Driver framework
* FAT Fs (Currently read only)
* execve (But only 1/3 to successfully run TvT)

What I'm doing:
* K210 Port (Use driver code from xv6-k210)
* Syscalls for comp

What I've not done:
* Time managment
