#ifndef STRING_H
#define STRING_H

#include "types.h"

/* Fill the first n bytes of dst with value c. */
void *memset(void *dst, int c, usize n);

/* Copy n bytes from src to dst (no-overlap). */
void *memcpy(void *dst, const void *src, usize n);

/* Copy n bytes from src to dst (overlap-safe). */
void *memmove(void *dst, const void *src, usize n);

/* Compare n bytes; return 0 if equal, <0 or >0 otherwise. */
int   memcmp(const void *a, const void *b, usize n);

/* Return length of null-terminated string s. */
usize strlen(const char *s);

/* Compare null-terminated strings; return 0 if equal. */
int   strcmp(const char *a, const char *b);

/* Copy null-terminated string src into dst (including NUL). */
char *strcpy(char *dst, const char *src);

/* Copy at most n bytes from src to dst, NUL-padding to n bytes. */
char *strncpy(char *dst, const char *src, usize n);

/* Compare at most n chars of a and b. */
int   strncmp(const char *a, const char *b, usize n);

#endif /* STRING_H */
