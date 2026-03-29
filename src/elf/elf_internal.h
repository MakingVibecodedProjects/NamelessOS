#ifndef ELF_INTERNAL_H
#define ELF_INTERNAL_H

#include "../lib/types.h"

/* ── ELF64 magic / class / data ──────────────────────────────────── */
#define ELFMAG0         0x7Fu
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'
#define ELFCLASS64      2u    /* 64-bit objects */
#define ELFDATA2LSB     1u    /* little-endian  */
#define ET_EXEC         2u    /* executable file */
#define EM_X86_64       62u   /* AMD x86-64 */

/* ── ELF64 header ────────────────────────────────────────────────── */
typedef struct {
    u8  e_ident[16];   /* magic, class, data, version, OS/ABI, padding */
    u16 e_type;        /* object file type */
    u16 e_machine;     /* target ISA */
    u32 e_version;     /* ELF version (always 1) */
    u64 e_entry;       /* virtual address of entry point */
    u64 e_phoff;       /* file offset of program header table */
    u64 e_shoff;       /* file offset of section header table (unused here) */
    u32 e_flags;       /* processor-specific flags */
    u16 e_ehsize;      /* size of this header (64 bytes) */
    u16 e_phentsize;   /* size of one program header entry */
    u16 e_phnum;       /* number of program header entries */
    u16 e_shentsize;   /* size of one section header entry */
    u16 e_shnum;       /* number of section header entries */
    u16 e_shstrndx;    /* section name string table index */
} __attribute__((packed)) elf64_hdr_t;

/* ── ELF64 program header ────────────────────────────────────────── */
typedef struct {
    u32 p_type;    /* segment type */
    u32 p_flags;   /* segment-dependent flags */
    u64 p_offset;  /* offset of segment in file */
    u64 p_vaddr;   /* virtual address in memory */
    u64 p_paddr;   /* physical address (usually same as p_vaddr) */
    u64 p_filesz;  /* size in file (may be < p_memsz) */
    u64 p_memsz;   /* size in memory */
    u64 p_align;   /* alignment (power of 2; 0/1 = no alignment) */
} __attribute__((packed)) elf64_phdr_t;

/* ── Program header types ────────────────────────────────────────── */
#define PT_NULL    0u   /* unused */
#define PT_LOAD    1u   /* loadable segment */
#define PT_DYNAMIC 2u   /* dynamic linking info */
#define PT_INTERP  3u   /* interpreter path */
#define PT_NOTE    4u   /* auxiliary info */

/* ── Program header flags ────────────────────────────────────────── */
#define PF_X  (1u << 0)   /* execute */
#define PF_W  (1u << 1)   /* write   */
#define PF_R  (1u << 2)   /* read    */

/* ── Sanity limits ───────────────────────────────────────────────── */
#define ELF_MAX_PHDRS  16u   /* max program headers we'll process */

#endif /* ELF_INTERNAL_H */
