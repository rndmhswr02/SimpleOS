#ifndef ULIB_H
#define ULIB_H

#include <stdint.h>
#include "../src/syscall_nr.h"     /* the same numbers the kernel uses */

/* Raw system call: number in eax, arguments in ebx, ecx, edx; result in eax. */
static inline uint32_t syscall3(uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t ret = num;
    __asm__ volatile ("int $0x80"
                      : "+a"(ret)
                      : "b"(a), "c"(b), "d"(c)
                      : "memory");
    return ret;
}

static inline __attribute__((noreturn)) void sys_exit(uint32_t code) {
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) { }
}

static inline uint32_t sys_write(uint32_t fd, const void *buf, uint32_t len) {
    return syscall3(SYS_WRITE, fd, (uint32_t)(uintptr_t)buf, len);
}

static inline uint32_t sys_getpid(void)        { return syscall3(SYS_GETPID, 0, 0, 0); }
static inline void     sys_sleep(uint32_t ms)  { syscall3(SYS_SLEEP, ms, 0, 0); }
static inline void     sys_yield(void)         { syscall3(SYS_YIELD, 0, 0, 0); }
static inline uint32_t sys_ticks(void)         { return syscall3(SYS_TICKS, 0, 0, 0); }

/* ---- tiny helpers ---- */

static inline uint32_t ustrlen(const char *s) {
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

static inline void print(const char *s) {
    sys_write(1, s, ustrlen(s));
}

static inline void print_uint(uint32_t v) {
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (v == 0)
        buf[--i] = '0';
    while (v > 0) {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    }
    print(&buf[i]);
}

#endif