#include "kstring.h"

int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstarts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

int kparse_uint(const char *s, uint32_t *out) {
    if (*s == '\0')
        return 0;

    uint32_t value = 0;
    while (*s) {
        if (*s < '0' || *s > '9')
            return 0;
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    *out = value;
    return 1;
}