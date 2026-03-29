#ifndef _STDDEF_H
#define _STDDEF_H

#include <stdint.h>

#define NULL    ((void *)0)
#define offsetof(type, member) __builtin_offsetof(type, member)

typedef uint64_t  size_t;
typedef int64_t   ptrdiff_t;

#endif /* _STDDEF_H */
