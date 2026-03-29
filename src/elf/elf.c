#include "elf.h"
#include "elf_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"
#include "../pmm/pmm.h"
#include "../vmm/vmm.h"
#include "../vmm/vmm_internal.h"

/* ── elf_check ───────────────────────────────────────────────────── */
static int elf_check(const elf64_hdr_t *hdr, usize buf_size) {
    if (buf_size < sizeof(elf64_hdr_t))                 return -1;
    if (hdr->e_ident[0] != ELFMAG0)                     return -1;
    if (hdr->e_ident[1] != ELFMAG1)                     return -1;
    if (hdr->e_ident[2] != ELFMAG2)                     return -1;
    if (hdr->e_ident[3] != ELFMAG3)                     return -1;
    if (hdr->e_ident[4] != ELFCLASS64)                  return -1;
    if (hdr->e_ident[5] != ELFDATA2LSB)                 return -1;
    if (hdr->e_type    != ET_EXEC)                      return -1;
    if (hdr->e_machine != EM_X86_64)                    return -1;
    if (hdr->e_phentsize < sizeof(elf64_phdr_t))        return -1;
    if (hdr->e_phnum   == 0)                            return -1;
    if (hdr->e_phnum   >  ELF_MAX_PHDRS)               return -1;
    /* Program header table must be inside the buffer */
    if (hdr->e_phoff + (u64)hdr->e_phnum * hdr->e_phentsize > buf_size)
        return -1;
    return 0;
}

/* ── elf_load_segment ────────────────────────────────────────────── */
/* Load one PT_LOAD segment into pml4_phys.
   Returns 0 on success, -1 on OOM or out-of-bounds. */
static int elf_load_segment(const u8 *buf, usize buf_size,
                             const elf64_phdr_t *ph, u64 pml4_phys) {
    if (ph->p_memsz == 0) return 0;

    /* Sanity: file data must be inside the buffer */
    if (ph->p_filesz > 0) {
        if (ph->p_offset + ph->p_filesz > buf_size) return -1;
    }

    /* Build PTE flags from segment flags */
    u64 pte_flags = PTE_USER;
    if (ph->p_flags & PF_W) pte_flags |= PTE_WRITE;
    if (!(ph->p_flags & PF_X)) pte_flags |= PTE_NX;

    /* Page-align the virtual address range */
    u64 vaddr_start = ph->p_vaddr & PAGE_MASK;
    u64 vaddr_end   = (ph->p_vaddr + ph->p_memsz + PAGE_SIZE - 1) & PAGE_MASK;

    /* File data window */
    u64 file_start = ph->p_offset;
    u64 file_end   = ph->p_offset + ph->p_filesz;

    u64 vaddr = vaddr_start;
    while (vaddr < vaddr_end) {
        u64 frame = pmm_alloc_frame();
        if (!frame) return -1;

        u8 *kva = (u8 *)(usize)frame;   /* identity-mapped */
        memset(kva, 0, PAGE_SIZE);

        /* Copy the slice of file data that falls in this page */
        u64 page_end = vaddr + PAGE_SIZE;
        /* Intersection of [vaddr, page_end) with [p_vaddr, p_vaddr+p_filesz) */
        u64 copy_va_start = vaddr  > ph->p_vaddr ? vaddr  : ph->p_vaddr;
        u64 copy_va_end   = page_end < ph->p_vaddr + ph->p_filesz
                            ? page_end
                            : ph->p_vaddr + ph->p_filesz;

        if (copy_va_start < copy_va_end) {
            u64 dst_off  = copy_va_start - vaddr;            /* offset into frame */
            u64 src_off  = file_start + (copy_va_start - ph->p_vaddr);
            u64 copy_len = copy_va_end - copy_va_start;
            if (src_off + copy_len <= buf_size)
                memcpy(kva + dst_off, buf + src_off, (usize)copy_len);
        }
        (void)file_end;   /* bounds checked above */

        if (vmm_map_user_page(pml4_phys, vaddr, frame, pte_flags) != 0) {
            pmm_free_frame(frame);
            return -1;
        }

        vaddr += PAGE_SIZE;
    }
    return 0;
}

/* ── elf_load ────────────────────────────────────────────────────── */
int elf_load(const u8 *buf, usize buf_size,
             u64 pml4_phys, u64 *entry_out) {
    const elf64_hdr_t *hdr = (const elf64_hdr_t *)buf;

    if (elf_check(hdr, buf_size) != 0) {
        klog(LOG_ERROR, "[elf] invalid ELF64 header");
        return -1;
    }

    /* Walk program headers */
    u16 loaded = 0;
    for (u16 i = 0; i < hdr->e_phnum; i++) {
        const elf64_phdr_t *ph = (const elf64_phdr_t *)
            (buf + hdr->e_phoff + (u64)i * hdr->e_phentsize);

        if (ph->p_type != PT_LOAD) continue;

        if (elf_load_segment(buf, buf_size, ph, pml4_phys) != 0) {
            klog(LOG_ERROR, "[elf] failed to load segment %u vaddr=0x%x",
                 (unsigned)i, (unsigned)ph->p_vaddr);
            return -1;
        }
        loaded++;
        klog(LOG_DEBUG, "[elf] loaded PT_LOAD vaddr=0x%x memsz=0x%x flags=%x",
             (unsigned)ph->p_vaddr, (unsigned)ph->p_memsz,
             (unsigned)ph->p_flags);
    }

    if (loaded == 0) {
        klog(LOG_ERROR, "[elf] no PT_LOAD segments found");
        return -1;
    }

    *entry_out = hdr->e_entry;
    klog(LOG_INFO, "[elf] loaded %u segment(s), entry=0x%x",
         (unsigned)loaded, (unsigned)hdr->e_entry);
    return 0;
}

/* ── elf_dump ────────────────────────────────────────────────────── */
static void elf_dump(void) {
    klog(LOG_DEBUG, "[elf] ELF64 loader ready (max %u phdrs)", ELF_MAX_PHDRS);
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_elf = {
    .name        = "elf",
    .initialized = false,
    .init        = NULL,    /* no init needed — pure library */
    .dump        = elf_dump,
    .shutdown    = NULL,
};
