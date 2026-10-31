#include "process.h"
#include "cpu.h"
#include "memops.h"
#include "paging.h"
#include "pmm.h"

#define USER_STACK_PAGES  2u
#define MAX_IMAGE_BYTES   (1024u * 1024u)

extern void enter_usermode(uint32_t entry, uint32_t user_esp) __attribute__((noreturn));

/* Runs as the process's kernel task, already inside its own address space.
 * It never returns: it drops to ring 3 and the CPU only comes back via interrupts. */
static void user_task_entry(void *arg) {
    (void)arg;
    enter_usermode(USER_BASE, USER_STACK_TOP);
}

/* Allocate a frame, fill it (zeros, then `len` bytes from `src`), map it at `virt`. */
static int map_user_page(uint32_t pd, uint32_t virt, const uint8_t *src, uint32_t len) {
    uint32_t frame = pmm_alloc_frame();
    if (!frame)
        return -1;

    /* The kernel can reach any frame directly because all RAM is identity-mapped. */
    uint8_t *dst = (uint8_t *)(uintptr_t)frame;
    memset(dst, 0, PAGE_SIZE);
    if (src && len)
        memcpy(dst, src, len);

    if (paging_map_page_in(pd, virt, frame, PAGE_WRITE | PAGE_USER) != 0) {
        pmm_free_frame(frame);
        return -1;
    }
    return 0;
}

struct task *process_spawn(const char *name, const uint8_t *image, uint32_t size) {
    if (size == 0 || size > MAX_IMAGE_BYTES)
        return 0;

    uint32_t pd = paging_create_address_space();
    if (!pd)
        return 0;

    /* Program image: one page at a time, starting at USER_BASE. */
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t p = 0; p < pages; p++) {
        uint32_t offset = p * PAGE_SIZE;
        uint32_t chunk  = size - offset;
        if (chunk > PAGE_SIZE)
            chunk = PAGE_SIZE;

        if (map_user_page(pd, USER_BASE + offset, image + offset, chunk) != 0)
            goto fail;
    }

    /* User stack: the pages just below USER_STACK_TOP (grows downward). */
    for (uint32_t p = 1; p <= USER_STACK_PAGES; p++) {
        if (map_user_page(pd, USER_STACK_TOP - p * PAGE_SIZE, 0, 0) != 0)
            goto fail;
    }

    /* Create the task and attach the address space BEFORE it can be scheduled:
     * a timer tick between the two steps would otherwise run it in the kernel's
     * address space. */
    uint32_t flags = irq_save();
    struct task *t = task_create(name, user_task_entry, 0);
    if (t)
        t->pd = pd;
    irq_restore(flags);

    if (!t)
        goto fail;
    return t;

fail:
    paging_destroy_address_space(pd);      /* frees whatever was mapped so far */
    return 0;
}