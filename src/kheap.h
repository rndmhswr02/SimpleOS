#ifndef KHEAP_H
#define KHEAP_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_START 0xD0000000u      /* virtual address where the heap begins */

struct kheap_stats {
    uint32_t mapped_bytes;          /* total virtual bytes currently backed by frames */
    uint32_t used_bytes;            /* payload bytes in allocated blocks */
    uint32_t used_blocks;
    uint32_t free_bytes;            /* payload bytes in free blocks */
    uint32_t free_blocks;
};

/* Map the initial heap pages and create the first free block. Call after paging_init(). */
void kheap_init(void);

void *kmalloc(size_t size);                 /* NULL on failure; result is 16-byte aligned */
void *kcalloc(size_t count, size_t size);   /* zero-filled */
void  kfree(void *ptr);                     /* kfree(NULL) is a no-op */

void kheap_stats(struct kheap_stats *out);
void kheap_dump(void);                      /* print every block (great for learning) */

#endif