#ifndef HEAP_INTERNAL_H
#define HEAP_INTERNAL_H

#include "../lib/types.h"

/* ── Slab allocator constants ────────────────────────────────────── */

/* Smallest and largest power-of-two object size handled by a slab cache.
   Objects outside this range fall back to page-granularity allocation. */
#define HEAP_MIN_ORDER   3   /* 8   bytes */
#define HEAP_MAX_ORDER   12  /* 4096 bytes */
#define HEAP_NUM_CACHES  (HEAP_MAX_ORDER - HEAP_MIN_ORDER + 1)  /* 10 */

/* Each slab is exactly one 4 KB page. */
#define SLAB_SIZE        4096ULL
#define SLAB_ALIGN       4096ULL

/* ── Free-object linked list node ───────────────────────────────── */
/* The first 8 bytes of every free object are repurposed as a pointer
   to the next free object within the same slab (or NULL). */
typedef struct free_obj {
    struct free_obj *next;
} free_obj_t;

/* ── Slab descriptor ─────────────────────────────────────────────── */
/* Stored at the very beginning of the slab page it describes. */
typedef struct slab {
    struct slab  *next;        /* next slab in same cache's list */
    free_obj_t   *freelist;    /* head of free-object list */
    u32           obj_size;    /* size of each object in bytes */
    u32           free_count;  /* number of free objects remaining */
    u32           total;       /* total objects in this slab */
    u32           _pad;        /* align to 32 bytes */
} slab_t;

/* ── Slab cache (one per object size) ───────────────────────────── */
typedef struct {
    u32       obj_size;        /* object size this cache serves */
    slab_t   *partial;         /* slabs with free slots */
    slab_t   *full;            /* slabs with no free slots */
} slab_cache_t;

/* ── Large allocation header (objects > HEAP_MAX_ORDER size) ─────── */
/* Prepended to every large allocation; the returned pointer points
   just past this header. */
typedef struct large_hdr {
    u64  num_pages;    /* number of 4 KB pages allocated */
} large_hdr_t;

#endif /* HEAP_INTERNAL_H */
