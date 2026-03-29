#include "string.h"

void *memset(void *dst, int c, usize n) {
    unsigned char *p = (unsigned char *)dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, usize n) {
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, usize n) {
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, usize n) {
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

usize strlen(const char *s) {
    usize len = 0;
    while (*s++) len++;
    return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

char *strncpy(char *dst, const char *src, usize n) {
    char *ret = dst;
    while (n && (*dst++ = *src++)) n--;
    while (n--) *dst++ = '\0';
    return ret;
}

int strncmp(const char *a, const char *b, usize n) {
    while (n--) {
        int diff = (int)(unsigned char)*a - (int)(unsigned char)*b;
        if (diff || !*a) return diff;
        a++; b++;
    }
    return 0;
}
