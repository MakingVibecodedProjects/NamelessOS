#include "dynlink.h"
#include "dynlink_internal.h"
#include "../lib/types.h"
#include "../lib/string.h"
#include "../serial/serial.h"
#include "../pmm/pmm.h"
#include "../vmm/vmm.h"
#include "../vmm/vmm_internal.h"
#include "../process/process.h"
#include "../syscall/syscall.h"

/* ── Per-process mmap bump pointer ──────────────────────────────────
   We track one mmap cursor per process.  Since we only have one user
   process at a time in practice, a single static is sufficient.
   A real kernel would store this in the process_t struct.            */
static u64 mmap_next = MMAP_BASE;

/* ── sys_mmap(addr, length, prot, flags, fd, offset) → va or -ENOMEM
   Only MAP_ANONYMOUS|MAP_PRIVATE is supported (fd=-1, offset=0).
   addr hint is ignored; we always use the bump pointer.             */
static u64 sys_mmap(u64 addr __attribute__((unused)),
                    u64 length,
                    u64 prot  __attribute__((unused)),
                    u64 flags,
                    u64 fd    __attribute__((unused)),
                    u64 offset __attribute__((unused))) {
    if (!(flags & MAP_ANONYMOUS))
        return MAP_FAILED;    /* file-backed mmap not supported */
    if (length == 0)
        return MAP_FAILED;

    /* Round up to page boundary */
    u64 pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Check we haven't exhausted the anonymous mapping window */
    if (mmap_next + pages * PAGE_SIZE > MMAP_LIMIT)
        return MAP_FAILED;

    process_t *p = process_current();
    if (!p || !p->pml4_phys)
        return MAP_FAILED;

    u64 va = mmap_next;

    for (u64 i = 0; i < pages; i++) {
        u64 frame = pmm_alloc_frame();
        if (!frame) return MAP_FAILED;
        memset((void *)PHYS_TO_VIRT(frame), 0, PAGE_SIZE);
        u64 vaddr = va + i * PAGE_SIZE;
        if (vmm_map_user_page(p->pml4_phys, vaddr, frame,
                              PTE_USER | PTE_WRITE) != 0) {
            pmm_free_frame(frame);
            return MAP_FAILED;
        }
    }

    mmap_next += pages * PAGE_SIZE;
    return va;
}

/* ── sys_munmap(addr, length) → 0
   Stubs out — pages are not reclaimed (bump allocator semantics).  */
static u64 sys_munmap(u64 addr    __attribute__((unused)),
                      u64 length  __attribute__((unused)),
                      u64 a2 __attribute__((unused)),
                      u64 a3 __attribute__((unused)),
                      u64 a4 __attribute__((unused)),
                      u64 a5 __attribute__((unused))) {
    return 0;
}

/* ── Module init / dump ─────────────────────────────────────────── */
static int dynlink_init(void) {
    mmap_next = MMAP_BASE;
    syscall_register(9,  sys_mmap);    /* SYS_MMAP   = 9  */
    syscall_register(11, sys_munmap);  /* SYS_MUNMAP = 11 */
    klog(LOG_INFO, "[dynlink] mmap/munmap ready (anon window 0x%x-0x%x)",
         (unsigned)MMAP_BASE, (unsigned)MMAP_LIMIT);
    return 0;
}

static void dynlink_dump(void) {
    klog(LOG_DEBUG, "[dynlink] mmap cursor=0x%x", (unsigned)mmap_next);
}

kernel_module_t mod_dynlink = {
    .name        = "dynlink",
    .init        = dynlink_init,
    .dump        = dynlink_dump,
    .initialized = false,
};
