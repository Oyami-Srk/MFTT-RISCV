//
// Created by shiroko on 22-5-2.
//

#ifndef __LIB_ELF_H__
#define __LIB_ELF_H__

// very simple impl, type from FreeBSD and Linux

#include <types.h>
#include <proc.h>

typedef uintptr_t Elf64_Addr;  // unsigned address
typedef uint64_t  Elf64_Off;   // unsigned offset
typedef uint16_t  Elf64_Half;  // unsigned half word
typedef uint32_t  Elf64_Word;  // unsigned word
typedef int       Elf64_Sword; // signed word
typedef uint64_t  Elf64_Xword; // extend word

#define ELF_NIDENT 16

typedef struct {
    char       e_ident[ELF_NIDENT];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off  e_phoff;
    Elf64_Off  e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
} Elf64_Ehdr;

enum Elf_Ident {
    EI_MAG0       = 0, // 0x7F
    EI_MAG1       = 1, // 'E'
    EI_MAG2       = 2, // 'L'
    EI_MAG3       = 3, // 'F'
    EI_CLASS      = 4, // Architecture (32/64)
    EI_DATA       = 5, // Byte Order
    EI_VERSION    = 6, // ELF Version
    EI_OSABI      = 7, // OS Specific
    EI_ABIVERSION = 8, // OS Specific
    EI_PAD        = 9  // Padding
};

#define ELFMAG0 0x7F // e_ident[EI_MAG0]
#define ELFMAG1 'E'  // e_ident[EI_MAG1]
#define ELFMAG2 'L'  // e_ident[EI_MAG2]
#define ELFMAG3 'F'  // e_ident[EI_MAG3]

// EI_DATA
#define ELFDATANONE (0) // Unknown Endian
#define ELFDATA2LSB (1) // Little Endian
#define ELFDATA2MSB (1) // Big Endian

#define ELFCLASSNONE (0) // Unknown Architecture
#define ELFCLASS32   (1) // 32-bit Architecture
#define ELFCLASS64   (2) // 64-bit Architecture

enum Elf_Type {
    ET_NONE = 0, // Unkown Type
    ET_REL  = 1, // Relocatable File
    ET_EXEC = 2  // Executable File
};

#define EM_386     (3)   // x86 Machine Type
#define EM_RISCV   (243) // RISC-V Machine Type
#define EV_CURRENT (1)   // ELF Current Version

typedef struct {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr;
    Elf64_Addr  p_paddr; // nouse
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} __attribute__((packed)) Elf64_Phdr;

enum Elf_PType {
    PT_NULL    = 0,
    PT_LOAD    = 1,
    PT_DYNAMIC = 2,
    PT_INTERP  = 3,
    PT_NOTE    = 4,
    PT_SHLIB   = 5,
    PT_PHDR    = 6,
    PT_LOPROC  = 0x70000000,
    PT_HIPROC  = 0x7FFFFFFF
};

enum Elf_PFlay {
    PF_X        = 1,
    PF_W        = 2,
    PF_R        = 4,
    PF_MASKOS   = 0x0FF00000,
    PF_MASKPROC = 0xF0000000,
};

typedef struct {
    Elf64_Word  sh_name;
    Elf64_Word  sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr  sh_addr;
    Elf64_Off   sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word  sh_link;
    Elf64_Word  sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} __attribute__((packed)) Elf64_Shdr;

// Read buffer for elf load.
typedef size_t (*elf_buffer_reader)(void *reader_data, uint64_t offset,
                                    char *target, size_t size);

bool elf_check_header(Elf64_Ehdr *header);
bool elf_load_to_process(
    proc_t *proc, elf_buffer_reader reader,
    void *reader_data); // Must ensure process is valid and clear.

#endif // __LIB_ELF_H__