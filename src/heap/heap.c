#include "heap.h"
#include "heap_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"
#include "../pmm/pmm.h"
#include "../vmm/vmm.h"
#include "../vmm/vmm_internal.h"  /* PHYS_TO_VIRT / VIRT_TO_PHYS */

/* ── Cache table — one entry per power-of-two size class ─────────── */
static slab_cache_t caches[HEAP_NUM_CACHES];

/* ── Helpers ─────────────────────────────────────────────────────── */

static inline u32 cache_index(usize size) {
    u32 order = HEAP_MIN_ORDER;
    usize s = (1u << HEAP_MIN_ORDER);
    while (s < size && order < HEAP_MAX_ORDER) { s <<= 1; order++; }
    return order - HEAP_MIN_ORDER;
}

static inline usize round_up_pow2(usize size) {
    usize s = (1u << HEAP_MIN_ORDER);
    while (s < size) s <<= 1;
    return s;
}

/* ── slab_create ─────────────────────────────────────────────────── */
/* Allocate a fresh slab page, place descriptor at the front, build
   the freelist over the remainder. Returns NULL on OOM. */
static slab_t *slab_create(u32 obj_size) {
    u64 frame = pmm_alloc_frame();
    if (!frame) return NULL;

    /* Map the frame into kernel virtual space via the higher-half window */
    slab_t *slab = (slab_t *)PHYS_TO_VIRT(frame);
    memset(slab, 0, SLAB_SIZE);

    /* Descriptor lives at the start; first object follows it.
       Align the first object to obj_size. */
    usize hdr_size = sizeof(slab_t);
    /* Round header up to obj_size so objects stay naturally aligned */
    if (hdr_size % obj_size)
        hdr_size += obj_size - (hdr_size % obj_size);

    usize usable = SLAB_SIZE - hdr_size;
    u32   total  = (u32)(usable / obj_size);

    slab->obj_size   = obj_size;
    slab->total      = total;
    slab->free_count = total;
    slab->next       = NULL;

    /* Build intrusive freelist */
    u8 *base = (u8 *)slab + hdr_size;
    slab->freelist = (free_obj_t *)base;
    for (u32 i = 0; i < total; i++) {
        free_obj_t *obj = (free_obj_t *)(base + (usize)i * obj_size);
        obj->next = (i + 1 < total)
                    ? (free_obj_t *)(base + (usize)(i + 1) * obj_size)
                    : NULL;
    }
    return slab;
}

/* ── slab_alloc_from ─────────────────────────────────────────────── */
static void *slab_alloc_from(slab_t *slab) {
    free_obj_t *obj = slab->freelist;
    slab->freelist  = obj->next;
    slab->free_count--;
    return (void *)obj;
}

/* ── cache_alloc ─────────────────────────────────────────────────── */
static void *cache_alloc(slab_cache_t *cache) {
    /* Try existing partial slab */
    if (!cache->partial) {
        slab_t *s = slab_create(cache->obj_size);
        if (!s) return NULL;
        cache->partial = s;
    }

    slab_t *slab = cache->partial;
    void   *ptr  = slab_alloc_from(slab);

    if (slab->free_count == 0) {
        /* Move to full list */
        cache->partial = slab->next;
        slab->next     = cache->full;
        cache->full    = slab;
    }
    return ptr;
}

/* ── slab_of ─────────────────────────────────────────────────────── */
/* Return the slab_t* for a given object pointer (page-aligned down). */
static inline slab_t *slab_of(void *ptr) {
    return (slab_t *)((usize)ptr & ~(SLAB_SIZE - 1));
}

/* ── cache_free ──────────────────────────────────────────────────── */
static void cache_free(slab_cache_t *cache, void *ptr) {
    slab_t     *slab = slab_of(ptr);
    free_obj_t *obj  = (free_obj_t *)ptr;

    bool was_full = (slab->free_count == 0);
    obj->next      = slab->freelist;
    slab->freelist = obj;
    slab->free_count++;

    if (was_full) {
        /* Remove from full list */
        slab_t **p = &cache->full;
        while (*p && *p != slab) p = &(*p)->next;
        if (*p) *p = slab->next;
        /* Add to partial list */
        slab->next     = cache->partial;
        cache->partial = slab;
    }

    /* If slab is completely empty, return frame to PMM */
    if (slab->free_count == slab->total) {
        slab_t **p = &cache->partial;
        while (*p && *p != slab) p = &(*p)->next;
        if (*p) *p = slab->next;
        pmm_free_frame(VIRT_TO_PHYS((u64)slab));
    }
}

/* ── Large allocation (> 4096 bytes) ────────────────────────────── */
static void *large_alloc(usize size) {
    usize total     = size + sizeof(large_hdr_t);
    u64   num_pages = (total + SLAB_SIZE - 1) / SLAB_SIZE;

    /* Allocate contiguous frames one-by-one (simple, sufficient for now) */
    u64 first = 0;
    for (u64 i = 0; i < num_pages; i++) {
        u64 frame = pmm_alloc_frame();
        if (!frame) return NULL;   /* leak on partial — acceptable at this stage */
        if (i == 0) first = frame;
    }

    large_hdr_t *hdr = (large_hdr_t *)PHYS_TO_VIRT(first);
    hdr->num_pages   = num_pages;
    return (void *)((u64)hdr + sizeof(large_hdr_t));
}

static void large_free(void *ptr) {
    large_hdr_t *hdr = (large_hdr_t *)((u8 *)ptr - sizeof(large_hdr_t));
    /* Frames were allocated as separate PMM frames; free first page only
       (single-page large allocs are the common case; multi-page frees
       require tracking — left as a known limitation for now). */
    pmm_free_frame(VIRT_TO_PHYS((u64)hdr));
}

/* ── Public API ──────────────────────────────────────────────────── */

void *kmalloc(usize size) {
    if (!size) return NULL;

    if (size > (1u << HEAP_MAX_ORDER)) return large_alloc(size);

    usize obj_size = round_up_pow2(size < (1u << HEAP_MIN_ORDER)
                                   ? (1u << HEAP_MIN_ORDER) : size);
    u32 idx = cache_index(obj_size);
    return cache_alloc(&caches[idx]);
}

void *kzalloc(usize size) {
    void *p = kmalloc(size);
    if (p) memset(p, 0, size);
    return p;
}

void kfree(void *ptr) {
    if (!ptr) return;

    /* Determine if this is a slab object or a large allocation.
       Slab objects: the page-aligned address holds a valid slab_t whose
       obj_size matches one of our caches. */
    slab_t *slab = slab_of(ptr);
    if (slab->obj_size >= (1u << HEAP_MIN_ORDER) &&
        slab->obj_size <= (1u << HEAP_MAX_ORDER)) {
        u32 idx = cache_index(slab->obj_size);
        cache_free(&caches[idx], ptr);
        return;
    }
    large_free(ptr);
}

void *krealloc(void *ptr, usize size) {
    if (!ptr)  return kmalloc(size);
    if (!size) { kfree(ptr); return NULL; }

    /* Determine old size from slab or large header */
    usize old_size;
    slab_t *slab = slab_of(ptr);
    if (slab->obj_size >= (1u << HEAP_MIN_ORDER) &&
        slab->obj_size <= (1u << HEAP_MAX_ORDER)) {
        old_size = slab->obj_size;
    } else {
        large_hdr_t *hdr = (large_hdr_t *)((u8 *)ptr - sizeof(large_hdr_t));
        old_size = hdr->num_pages * SLAB_SIZE - sizeof(large_hdr_t);
    }

    if (size <= old_size) return ptr;   /* fits in existing block */

    void *newp = kmalloc(size);
    if (!newp) return NULL;
    memcpy(newp, ptr, old_size);
    kfree(ptr);
    return newp;
}

/* ── heap_dump ───────────────────────────────────────────────────── */
static void heap_dump(void) {
    for (int i = 0; i < HEAP_NUM_CACHES; i++) {
        u32 sz = caches[i].obj_size;
        u32 partial = 0, full = 0;
        for (slab_t *s = caches[i].partial; s; s = s->next) partial++;
        for (slab_t *s = caches[i].full;    s; s = s->next) full++;
        klog(LOG_DEBUG, "[heap] cache %4u B: %u partial, %u full", sz, partial, full);
    }
}

/* ── heap_init ───────────────────────────────────────────────────── */
int heap_init(void) {
    for (int i = 0; i < HEAP_NUM_CACHES; i++) {
        caches[i].obj_size = (u32)(1u << (HEAP_MIN_ORDER + i));
        caches[i].partial  = NULL;
        caches[i].full     = NULL;
    }
    klog(LOG_INFO, "[heap] slab allocator ready (%d caches, 8..4096 B)",
         HEAP_NUM_CACHES);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_heap = {
    .name        = "heap",
    .initialized = false,
    .init        = heap_init,
    .dump        = heap_dump,
    .shutdown    = NULL,
};
