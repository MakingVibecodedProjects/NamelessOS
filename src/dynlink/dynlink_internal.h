#ifndef DYNLINK_INTERNAL_H
#define DYNLINK_INTERNAL_H

/* mmap prot flags (Linux ABI) */
#define PROT_NONE   0
#define PROT_READ   1
#define PROT_WRITE  2
#define PROT_EXEC   4

/* mmap flags (Linux ABI) */
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

/* mmap failure sentinel */
#define MAP_FAILED  ((u64)(i64)(-1))

/* Userspace anonymous mapping region:
   0x10000000 – 0x4FFFFFFF (1 GB window, well below stack at 0x7FFFF000) */
#define MMAP_BASE   0x10000000ULL
#define MMAP_LIMIT  0x50000000ULL

#endif /* DYNLINK_INTERNAL_H */
