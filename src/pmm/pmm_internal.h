#ifndef PMM_INTERNAL_H
#define PMM_INTERNAL_H

#include "../lib/types.h"

/* ── Frame constants ─────────────────────────────────────────────── */
#define FRAME_SIZE       4096ULL
#define FRAME_SHIFT      12

/* ── Bitmap helpers ──────────────────────────────────────────────── */
/* Each u64 word holds 64 bits (frames).  Bit=0 means frame is FREE. */
#define BITMAP_WORD_BITS  64ULL
#define BITMAP_IDX(frame) ((frame) / BITMAP_WORD_BITS)
#define BITMAP_BIT(frame) ((frame) % BITMAP_WORD_BITS)

static inline void bitmap_set(u64 *bm, u64 frame) {
    bm[BITMAP_IDX(frame)] |= (1ULL << BITMAP_BIT(frame));
}
static inline void bitmap_clear(u64 *bm, u64 frame) {
    bm[BITMAP_IDX(frame)] &= ~(1ULL << BITMAP_BIT(frame));
}
static inline bool bitmap_test(const u64 *bm, u64 frame) {
    return (bm[BITMAP_IDX(frame)] >> BITMAP_BIT(frame)) & 1ULL;
}

/* ── PMM state ───────────────────────────────────────────────────── */
/* Maximum physical memory we track: 4 GB (1 M frames) */
#define PMM_MAX_FRAMES   (4ULL * 1024ULL * 1024ULL * 1024ULL / FRAME_SIZE)
#define PMM_BITMAP_WORDS (PMM_MAX_FRAMES / BITMAP_WORD_BITS)

typedef struct {
    u64  bitmap[PMM_BITMAP_WORDS];  /* 1 bit per 4K frame, 1=used 0=free */
    u64  total_frames;
    u64  free_frames;
    u64  mb2_info_phys;             /* physical address of MB2 struct     */
} pmm_state_t;

/* ── Multiboot2 tag types we care about ─────────────────────────── */
#define MB2_TAG_END      0
#define MB2_TAG_MMAP     6

/* Multiboot2 info header */
typedef struct {
    u32 total_size;
    u32 reserved;
} __attribute__((packed)) mb2_header_t;

/* Generic tag header */
typedef struct {
    u32 type;
    u32 size;
} __attribute__((packed)) mb2_tag_t;

/* Memory map tag */
typedef struct {
    u32 type;
    u32 size;
    u32 entry_size;
    u32 entry_version;
} __attribute__((packed)) mb2_tag_mmap_t;

/* Memory map entry */
typedef struct {
    u64 base_addr;
    u64 length;
    u32 mem_type;   /* 1 = available RAM */
    u32 reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

#define MB2_MMAP_AVAILABLE 1

/* ── Kernel image end boundary (provided by linker) ─────────────── */
/* Physical layout starts at KERNEL_PHYS (0x100000) and ends at the VA
   of __kernel_end mapped back to physical via − KERNEL_VMA. */
extern u8 __kernel_end[];     /* one byte past last VA of kernel image */

#endif /* PMM_INTERNAL_H */
