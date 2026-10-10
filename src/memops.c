#include "memops.h"
#include <stdint.h>

void *memset(void *dst, int c, size_t n) {
    void *ret = dst;
    __asm__ volatile ("rep stosb"
                      : "+D"(dst), "+c"(n)
                      : "a"(c)
                      : "memory");
    return ret;
}

void *memcpy(void *dst, const void *src, size_t n) {
    void *ret = dst;
    __asm__ volatile ("rep movsb"
                      : "+D"(dst), "+S"(src), "+c"(n)
                      :
                      : "memory");
    return ret;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;

    if (d == s || n == 0)
        return dst;
    if (d < s)
        return memcpy(dst, src, n);          /* forward copy is safe */

    /* dst overlaps the end of src: copy backwards */
    d += n - 1;
    s += n - 1;
    __asm__ volatile ("std\n\trep movsb\n\tcld"
                      : "+D"(d), "+S"(s), "+c"(n)
                      :
                      : "memory");
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = a;
    const uint8_t *y = b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i])
            return (int)x[i] - (int)y[i];
    }
    return 0;
}