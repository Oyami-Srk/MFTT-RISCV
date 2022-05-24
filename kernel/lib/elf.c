//
// Created by shiroko on 22-5-3.
//

#include <driver/console.h>
#include <lib/elf.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <memory.h>

#define ERROR(s, ...) kprintf("[ELF] Error: " s "\n", ##__VA_ARGS__)

bool elf_check_header(Elf64_Ehdr *header) {
    if (!header)
        return false;
    static const uint8_t elf_magic[4] = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3};
    if (memcmp(header->e_ident, elf_magic, 4) != 0) {
        ERROR("ELF Magic incorrect.");
        return false;
    }
    if (header->e_ident[EI_CLASS] != ELFCLASS64) {
        ERROR("Unsupported ELF Class.");
        return false;
    }
    if (header->e_ident[EI_DATA] != ELFDATA2LSB) {
        ERROR("Unsupported ELF Byte Order.");
        return false;
    }
    if (header->e_ident[EI_VERSION] != EV_CURRENT) {
        ERROR("Unsupported ELF Version.");
        return false;
    }
    if (header->e_machine != EM_RISCV) {
        ERROR("Unsupported Machine.");
        return false;
    }
    if (header->e_type != ET_EXEC) {
        ERROR("Unsupported ELF Type. Currently only support EXEC Type.");
        return false;
    }
    return true;
}

/*
 * typedef size_t (*elf_buffer_reader)(void *reader_data, uint64_t offset,
 *                                  char *target, size_t size);
 */
bool elf_load_to_process(proc_t *proc, elf_buffer_reader reader,
                         void *reader_data) {
    Elf64_Ehdr E_header;
    reader(reader_data, 0, (char *)&E_header, sizeof(Elf64_Ehdr));
    bool is_valid = elf_check_header(&E_header);
    if (!is_valid)
        return false;
    proc->prog_image_start = (char *)0xFFFFFFFFFFFFFFFF;
    proc->prog_break       = (char *)0;
    proc->prog_size        = 0;

    // Load Prog Header
    if (sizeof(Elf64_Phdr) != E_header.e_phentsize) {
        ERROR("Elf Prog Header size mismatch.");
        return false;
    }
    if (sizeof(Elf64_Shdr) != E_header.e_shentsize) {
        ERROR("Elf Section Header size mismatch.");
        return false;
    }
    Elf64_Phdr P_header;
    for (int i = 0; i < E_header.e_phnum; i++) {
        reader(reader_data, E_header.e_phoff + i * sizeof(Elf64_Phdr),
               (char *)&P_header, sizeof(Elf64_Phdr));
        if (P_header.p_type == PT_LOAD) {
            // Only load the load type prog header.
            if ((uintptr_t)proc->prog_image_start > P_header.p_vaddr) {
                proc->prog_image_start = (char *)P_header.p_vaddr;
            }
            if ((uintptr_t)proc->prog_break <
                P_header.p_vaddr + P_header.p_memsz) {
                proc->prog_break = (char *)P_header.p_vaddr + P_header.p_memsz;
            }
            size_t memsz = P_header.p_memsz;
            memsz        = PG_ROUNDUP(memsz);

            char *pa =
                page_alloc(memsz / PG_SIZE, PAGE_TYPE_INUSE | PAGE_TYPE_USER);
            char    *va        = (char *)P_header.p_vaddr;
            uint64_t va_offset = (uintptr_t)va - PG_ROUNDDOWN(va);

            reader(reader_data, P_header.p_offset, pa + va_offset,
                   P_header.p_filesz);
            if (va_offset)
                memset(pa, 0, va_offset);
            if (P_header.p_filesz != P_header.p_memsz) {
                if (P_header.p_filesz > P_header.p_memsz) {
                    // TODO: Clean-up and non panic, return false.
                    kpanic("[ELF] Prog header filesz larger than memsz.");
                }
                memset(pa + va_offset + P_header.p_filesz, 0,
                       P_header.p_memsz - P_header.p_filesz);
            }
            int pg_type = 0;
            if (P_header.p_flags & PF_R)
                pg_type |= PTE_TYPE_BIT_R;
            if (P_header.p_flags & PF_W)
                pg_type |= PTE_TYPE_BIT_W;
            if (P_header.p_flags & PF_X)
                pg_type |= PTE_TYPE_BIT_X;

            // TODO: Clean-up and non panic-assert, return false.
            assert(pg_type == PTE_TYPE_RWX || pg_type == PTE_TYPE_RW ||
                       pg_type == PTE_TYPE_XO || pg_type == PTE_TYPE_RO ||
                       pg_type == PTE_TYPE_RX,
                   "Elf Program memory type unsupported.");

            map_pages(proc->page_dir, (void *)PG_ROUNDDOWN(va), pa,
                      P_header.p_memsz, pg_type, true, false);
        }
    }
    proc->prog_size = proc->prog_break - proc->prog_image_start;
    proc->user_pc   = (void *)E_header.e_entry;
    return true;
}
