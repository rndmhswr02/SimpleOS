#include "syscall.h"
#include "console.h"
#include "kprintf.h"
#include "paging.h"
#include "sched.h"
#include "timer.h"

#define WRITE_MAX 256u

/* write(fd, buf, len): the buffer is a USER pointer, so it must be validated first. */
static uint32_t sys_write(const struct task *t, uint32_t fd, uint32_t buf, uint32_t len) {
    if (fd != 1)
        return (uint32_t)-1;
    if (len > WRITE_MAX)
        len = WRITE_MAX;                          /* write at most 256 bytes per call */
    if (!t->pd || !paging_user_range_ok(t->pd, buf, len, 0))
        return (uint32_t)-1;                      /* bad pointer: refuse, don't crash */

    /* Copy into a kernel buffer, then print. The range check above guarantees the
     * copy can't fault, and printing from the copy keeps user memory out of kprint. */
    char tmp[WRITE_MAX + 1];
    const char *src = (const char *)(uintptr_t)buf;
    for (uint32_t i = 0; i < len; i++)
        tmp[i] = src[i];
    tmp[len] = '\0';

    kprint(tmp);
    return len;
}

void syscall_dispatch(struct registers *r) {
    struct task *t = task_current();

    switch (r->eax) {
    case SYS_EXIT:
        kprintf("[process %u '%s'] exited with code %d\n", t->id, t->name, (int)r->ebx);
        task_exit();                              /* does not return */

    case SYS_WRITE:
        r->eax = sys_write(t, r->ebx, r->ecx, r->edx);
        break;

    case SYS_GETPID:
        r->eax = t->id;
        break;

    case SYS_SLEEP:
        task_sleep_ms(r->ebx);                    /* other tasks run while we sleep */
        r->eax = 0;
        break;

    case SYS_YIELD:
        task_yield();
        r->eax = 0;
        break;

    case SYS_TICKS:
        r->eax = timer_ticks();
        break;

    default:
        r->eax = (uint32_t)-1;                    /* unknown system call */
        break;
    }
}