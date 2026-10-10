#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT  0x001u
#define PAGE_WRITE    0x002u
#define PAGE_USER     0x004u

/* Build the kernel page directory (identity-mapping all RAM), then turn paging on.
 * Returns how many MiB were identity-mapped. Call after pmm_init(). */
uint32_t paging_init(void);

/* Map one 4 KiB page: virtual `virt` -> physical `phys`.
 * `flags` is a combination of PAGE_WRITE / PAGE_USER (PAGE_PRESENT is implied).
 * Returns 0 on success, -1 if a page table could not be allocated. */
int paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

/* Remove a mapping. Returns the physical address that was mapped, or 0 if none. */
uint32_t paging_unmap_page(uint32_t virt);

/* Translate a virtual address. Returns 0 if it isn't mapped. */
uint32_t paging_get_phys(uint32_t virt);

#endif