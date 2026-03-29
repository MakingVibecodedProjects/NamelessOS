#include "pmm.h"
#include "pmm_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"

/* ── PMM state (static — lives in .bss, zeroed at startup) ──────── */
static pmm_state_t pmm;

/* ── KERNEL_VMA offset (matches kernel.ld) ──────────────────────── */
#define KERNEL_VMA 0xFFFFFFFF80000000ULL

/* ── pmm_set_mb2 ─────────────────────────────────────────────────── */
void pmm_set_mb2(u64 mb2_info_phys) {
    pmm.mb2_info_phys = mb2_info_phys;
}

/* ── phys_to_virt: map a low physical address to its identity VA ── */
static inline void *phys_to_virt(u64 phys) {
    return (void *)(phys);   /* identity mapped for first 4 GB */
}

/* ── mark_range_used ─────────────────────────────────────────────── */
static void mark_range_used(u64 base, u64 length) {
    u64 frame_start = base / FRAME_SIZE;
    u64 frame_end   = (base + length + FRAME_SIZE - 1) / FRAME_SIZE;
    if (frame_end > PMM_MAX_FRAMES) frame_end = PMM_MAX_FRAMES;
    for (u64 f = frame_start; f < frame_end; f++) {
        if (!bitmap_test(pmm.bitmap, f)) {
            bitmap_set(pmm.bitmap, f);
            if (pmm.free_frames > 0) pmm.free_frames--;
        }
    }
}

/* ── mark_range_free ─────────────────────────────────────────────── */
static void mark_range_free(u64 base, u64 length) {
    u64 frame_start = base / FRAME_SIZE;
    u64 frame_end   = (base + length) / FRAME_SIZE;
    if (frame_end > PMM_MAX_FRAMES) frame_end = PMM_MAX_FRAMES;
    for (u64 f = frame_start; f < frame_end; f++) {
        if (bitmap_test(pmm.bitmap, f)) {
            bitmap_clear(pmm.bitmap, f);
            pmm.free_frames++;
        }
    }
}

/* ── pmm_init ────────────────────────────────────────────────────── */
int pmm_init(void) {
    /* Start with all frames marked used */
    memset(pmm.bitmap, 0xFF, sizeof(pmm.bitmap));
    pmm.total_frames = PMM_MAX_FRAMES;
    pmm.free_frames  = 0;

    /* Locate the Multiboot2 memory map tag */
    u64 mb2_phys = pmm.mb2_info_phys;
    if (!mb2_phys) {
        klog(LOG_ERROR, "[pmm] No Multiboot2 info pointer — cannot map memory");
        return -1;
    }

    mb2_header_t *hdr = (mb2_header_t *)phys_to_virt(mb2_phys);
    u64 usable_bytes  = 0;
    int region_count  = 0;

    /* Walk tags */
    u64 tag_addr = mb2_phys + sizeof(mb2_header_t);
    u64 end_addr = mb2_phys + hdr->total_size;

    while (tag_addr < end_addr) {
        mb2_tag_t *tag = (mb2_tag_t *)phys_to_virt(tag_addr);

        if (tag->type == MB2_TAG_END)
            break;

        if (tag->type == MB2_TAG_MMAP) {
            mb2_tag_mmap_t *mmap = (mb2_tag_mmap_t *)phys_to_virt(tag_addr);
            u64 entry_addr = tag_addr + sizeof(mb2_tag_mmap_t);
            u64 mmap_end   = tag_addr + mmap->size;

            while (entry_addr < mmap_end) {
                mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)phys_to_virt(entry_addr);
                if (e->mem_type == MB2_MMAP_AVAILABLE && e->base_addr > 0) {
                    mark_range_free(e->base_addr, e->length);
                    usable_bytes += e->length;
                    region_count++;
                }
                entry_addr += mmap->entry_size;
            }
        }

        /* Tags are 8-byte aligned */
        tag_addr += (tag->size + 7) & ~7ULL;
    }

    /* Re-mark frame 0 (null page) as used */
    bitmap_set(pmm.bitmap, 0);
    if (pmm.free_frames > 0) pmm.free_frames--;

    /* Re-mark the entire kernel load area as used.
       The physical layout starts at 0x100000 (KERNEL_PHYS): the .boot
       section with entry code and page tables lives there, followed by
       the higher-half kernel image (.text/.data/.bss).  We must cover
       the whole range 0x100000 → __kernel_end − KERNEL_VMA so the slab
       allocator never recycles frames that hold our page tables. */
    u64 kend_phys   = (u64)__kernel_end - KERNEL_VMA;
    mark_range_used(0x100000ULL, kend_phys - 0x100000ULL);

    /* Re-mark the bitmap itself as used (it's inside the kernel image,
       so already covered — but guard explicitly) */
    u64 bm_phys = (u64)pmm.bitmap - KERNEL_VMA;
    mark_range_used(bm_phys, sizeof(pmm.bitmap));

    u64 free_mb = (pmm.free_frames * FRAME_SIZE) / (1024ULL * 1024ULL);
    klog(LOG_INFO, "[pmm] %u MB free across %d usable region(s)",
         (unsigned int)free_mb, region_count);
    return 0;
}

/* ── pmm_alloc_frame ─────────────────────────────────────────────── */
u64 pmm_alloc_frame(void) {
    for (u64 w = 0; w < PMM_BITMAP_WORDS; w++) {
        if (pmm.bitmap[w] == ~0ULL) continue;   /* all used, skip word */
        /* Find first free bit in word */
        u64 word = pmm.bitmap[w];
        for (u64 b = 0; b < BITMAP_WORD_BITS; b++) {
            if (!((word >> b) & 1ULL)) {
                u64 frame = w * BITMAP_WORD_BITS + b;
                if (frame >= PMM_MAX_FRAMES) return 0;
                bitmap_set(pmm.bitmap, frame);
                pmm.free_frames--;
                return frame * FRAME_SIZE;
            }
        }
    }
    return 0;   /* out of memory */
}

/* ── pmm_free_frame ──────────────────────────────────────────────── */
void pmm_free_frame(u64 phys) {
    u64 frame = phys / FRAME_SIZE;
    if (frame >= PMM_MAX_FRAMES) return;
    if (!bitmap_test(pmm.bitmap, frame)) return;   /* double-free guard */
    bitmap_clear(pmm.bitmap, frame);
    pmm.free_frames++;
}

/* ── pmm_dump ────────────────────────────────────────────────────── */
static void pmm_dump(void) {
    u64 free_mb = (pmm.free_frames * FRAME_SIZE) / (1024ULL * 1024ULL);
    klog(LOG_DEBUG, "[pmm] %u frames free (%u MB)",
         (unsigned int)pmm.free_frames, (unsigned int)free_mb);
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_pmm = {
    .name        = "pmm",
    .initialized = false,
    .init        = pmm_init,
    .dump        = pmm_dump,
    .shutdown    = NULL,
};
