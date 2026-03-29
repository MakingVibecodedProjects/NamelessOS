#ifndef TYPES_H
#define TYPES_H

/* Fixed-width integer typedefs — no hosted libc */
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef unsigned long       usize;

typedef signed char         i8;
typedef signed short        i16;
typedef signed int          i32;
typedef signed long long    i64;

typedef u8                  bool;
#define true  ((bool)1)
#define false ((bool)0)

#define NULL ((void *)0)

#endif /* TYPES_H */
