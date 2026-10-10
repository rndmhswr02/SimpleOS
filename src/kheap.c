#include "kheap.h"
#include "kprintf.h"
#include "memops.h"
#include "paging.h"
#include "pmm.h"

#define HEAP_LIMIT    0xD4000000u              /* 64 MiB maximum */
#define HEAP_INITIAL  (16u * PAGE_SIZE)        /* 64 KiB mapped at startup */
#define ALIGNMENT     16u
#define MAGIC_USED    0xA110CA7Eu
#define MAGIC_FREE    0xF4EEB10Cu

/* Header in front of every block. 16 bytes, so payloads stay 16-byte aligned. */
struct block {
    uint32_t size;               /* payload size in bytes (a multiple of 16) */
    uint32_t magic;              /* MAGIC_USED or MAGIC_FREE */
    struct block *next;          /* next block in address order */
    struct block *prev;          /* previous block in address order */
};

#define HDR ((uint32_t)sizeof(struct block))

static struct block *head;       /* first block */
static struct block *tail;       /* last block */
static uint32_t heap_end;        /* first virtual address NOT yet mapped */

static void heap_panic(const char *msg, const struct block *b) {
    kprintf("\nHEAP PANIC: %s (block at %p)\n", msg, (const void *)b);
    for (;;)
        __asm__ volatile ("cli; hlt");
}

/* Map enough new pages to fit a block with `min_payload` bytes of payload. */
static int heap_grow(uint32_t min_payload) {
    uint32_t need  = min_payload + HDR;
    uint32_t pages = (need + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t added = pages * PAGE_SIZE;

    if (heap_end + added > HEAP_LIMIT)
        return -1;

    for (uint32_t p = 0; p < pages; p++) {
        uint32_t virt  = heap_end + p * PAGE_SIZE;
        uint32_t frame = pmm_alloc_frame();

        if (!frame || paging_map_page(virt, frame, PAGE_WRITE) != 0) {
            if (frame)
                pmm_free_frame(frame);
            /* Roll back the pages mapped so far in this call. */
            for (uint32_t q = 0; q < p; q++) {
                uint32_t phys = paging_unmap_page(heap_end + q * PAGE_SIZE);
                if (phys)
                    pmm_free_frame(phys);
            }
            return -1;
        }
    }

    if (tail && tail->magic == MAGIC_FREE) {
        tail->size += added;                         /* extend the trailing free block */
    } else {
        struct block *b = (struct block *)(uintptr_t)heap_end;
        b->size  = added - HDR;
        b->magic = MAGIC_FREE;
        b->prev  = tail;
        b->next  = 0;
        if (tail)
            tail->next = b;
        else
            head = b;
        tail = b;
    }

    heap_end += added;
    return 0;
}

void kheap_init(void) {
    head = 0;
    tail = 0;
    heap_end = HEAP_START;

    if (heap_grow(HEAP_INITIAL - HDR) != 0)
        heap_panic("could not map the initial heap", 0);
}

void *kmalloc(size_t size) {
    if (size == 0 || size > HEAP_LIMIT - HEAP_START)
        return 0;

    uint32_t n = ((uint32_t)size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

    for (int attempt = 0; attempt < 2; attempt++) {
        for (struct block *b = head; b; b = b->next) {
            if (b->magic != MAGIC_FREE && b->magic != MAGIC_USED)
                heap_panic("corrupted block header", b);

            if (b->magic == MAGIC_FREE && b->size >= n) {
                /* Split if the leftover can hold a header plus a minimal payload. */
                if (b->size >= n + HDR + ALIGNMENT) {
                    struct block *rest = (struct block *)((uint8_t *)b + HDR + n);
                    rest->size  = b->size - n - HDR;
                    rest->magic = MAGIC_FREE;
                    rest->prev  = b;
                    rest->next  = b->next;
                    if (rest->next)
                        rest->next->prev = rest;
                    else
                        tail = rest;
                    b->next = rest;
                    b->size = n;
                }
                b->magic = MAGIC_USED;
                return (uint8_t *)b + HDR;
            }
        }

        /* Nothing fit: map more pages and scan again. */
        if (heap_grow(n) != 0)
            return 0;
    }
    return 0;
}

void *kcalloc(size_t count, size_t size) {
    if (count != 0 && size > 0xFFFFFFFFu / count)
        return 0;                                    /* multiplication would overflow */

    void *p = kmalloc(count * size);
    if (p)
        memset(p, 0, count * size);
    return p;
}

void kfree(void *ptr) {
    if (!ptr)
        return;

    uintptr_t addr = (uintptr_t)ptr;
    if (addr < HEAP_START + HDR || addr >= heap_end || (addr & (ALIGNMENT - 1))) {
        kprintf("kfree: %p is not a heap pointer\n", ptr);
        return;
    }

    struct block *b = (struct block *)((uint8_t *)ptr - HDR);

    if (b->magic == MAGIC_FREE) {
        kprintf("kfree: double free of %p\n", ptr);
        return;
    }
    if (b->magic != MAGIC_USED) {
        kprintf("kfree: %p has a corrupted header (bad pointer or buffer overflow)\n", ptr);
        return;
    }

    b->magic = MAGIC_FREE;

    /* Coalesce with the next block if it is free. */
    if (b->next && b->next->magic == MAGIC_FREE) {
        struct block *n = b->next;
        b->size += HDR + n->size;
        b->next = n->next;
        if (n->next)
            n->next->prev = b;
        else
            tail = b;
        n->magic = 0;
    }

    /* Coalesce with the previous block if it is free. */
    if (b->prev && b->prev->magic == MAGIC_FREE) {
        struct block *p = b->prev;
        p->size += HDR + b->size;
        p->next = b->next;
        if (b->next)
            b->next->prev = p;
        else
            tail = p;
        b->magic = 0;
    }
}

void kheap_stats(struct kheap_stats *out) {
    out->mapped_bytes = heap_end - HEAP_START;
    out->used_bytes = out->used_blocks = 0;
    out->free_bytes = out->free_blocks = 0;

    for (const struct block *b = head; b; b = b->next) {
        if (b->magic == MAGIC_USED) {
            out->used_bytes += b->size;
            out->used_blocks++;
        } else {
            out->free_bytes += b->size;
            out->free_blocks++;
        }
    }
}

void kheap_dump(void) {
    kprintf("Heap blocks (16-byte header each):\n");
    for (const struct block *b = head; b; b = b->next) {
        kprintf("  %p: %s %u bytes\n", (const void *)b,
                b->magic == MAGIC_USED ? "used" : "free", b->size);
    }
}