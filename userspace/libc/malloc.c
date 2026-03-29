#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

/* ── brk-based bump allocator ────────────────────────────────────── */
/* We call the SYS_BRK syscall to extend the data segment.
   This is a simple bump allocator: free() is a no-op.
   Phase 6 Step 4 will replace this with a proper heap. */

extern void *brk_syscall(void *addr);

static uintptr_t heap_start = 0;
static uintptr_t heap_cur   = 0;
static uintptr_t heap_end   = 0;

#define HEAP_INIT_SIZE  (64 * 1024)     /* 64 KB initial heap */
#define HEAP_GROW_SIZE  (64 * 1024)     /* grow by 64 KB each time */
#define ALIGN8(x)       (((x) + 7ULL) & ~7ULL)

static int heap_init(void) {
    /* Ask kernel for current brk by passing 0 */
    uintptr_t cur = (uintptr_t)brk_syscall((void *)0);
    if (!cur) return -1;
    heap_start = cur;
    heap_cur   = cur;
    /* Grow by HEAP_INIT_SIZE */
    uintptr_t new_end = (uintptr_t)brk_syscall((void *)(cur + HEAP_INIT_SIZE));
    if (new_end < cur + HEAP_INIT_SIZE) return -1;
    heap_end = new_end;
    return 0;
}

static int heap_grow(size_t needed) {
    size_t grow = needed > HEAP_GROW_SIZE ? needed : HEAP_GROW_SIZE;
    uintptr_t new_end = (uintptr_t)brk_syscall((void *)(heap_end + grow));
    if (new_end < heap_end + grow) return -1;
    heap_end = new_end;
    return 0;
}

void *malloc(size_t size) {
    if (!size) return (void *)0;
    if (!heap_start && heap_init() != 0) return (void *)0;

    size = ALIGN8(size);
    if (heap_cur + size > heap_end) {
        if (heap_grow(size) != 0) return (void *)0;
    }
    void *ptr  = (void *)heap_cur;
    heap_cur  += size;
    return ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) {
        /* zero the memory */
        unsigned char *b = (unsigned char *)p;
        for (size_t i = 0; i < total; i++) b[i] = 0;
    }
    return p;
}

void *realloc(void *ptr, size_t size) {
    /* Bump allocator: always allocate new; old block leaks */
    void *new = malloc(size);
    if (new && ptr) {
        /* We don't know the old size — copy up to `size` bytes.
           This is safe only for grow-style realloc. */
        unsigned char *s = (unsigned char *)ptr;
        unsigned char *d = (unsigned char *)new;
        for (size_t i = 0; i < size; i++) d[i] = s[i];
    }
    return new;
}

/* free is a no-op in the bump allocator */
void free(void *ptr) {
    (void)ptr;
}

void exit(int status) {
    _exit(status);
}

int atoi(const char *s) {
    int n = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}

long atol(const char *s) {
    long n = 0; int neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}
