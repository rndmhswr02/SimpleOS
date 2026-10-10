#ifndef KSTRING_H
#define KSTRING_H
#include <stdint.h>

int kstrcmp(const char *a, const char *b);
int kstarts_with(const char *s, const char *prefix);
/* Parse an unsigned decimal number. Returns 1 on success (value stored in *out), 0 on bad input. */
int kparse_uint(const char *s, uint32_t *out);

#endif