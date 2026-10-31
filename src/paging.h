#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT  0x001u
#define PAGE_WRITE    0x002u
#define PAGE_USER     0x004u

/* User virtual address layout */
#define USER_BASE       0x40000000u   /* program image is loaded here */
#define USER_STACK_TOP  0x80000000u   /* initial user ESP */
#define USER_LIMIT      0x80000000u   /* user pointers must stay below this */

/* Build the kernel page directory (identity-mapping all RAM), then turn paging on.
 * Returns how many MiB were identity-mapped. Call after pmm_init(). */
uint32_t paging_init(void);

/* ---- kernel address space ---- */

int      paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
uint32_t paging_unmap_page(uint32_t virt);
uint32_t paging_get_phys(uint32_t virt);

/* Make sure page tables exist for every 4 MiB region in [start, end) of the kernel
 * address space. Must be done BEFORE processes are created, because every process
 * copies the kernel's page-directory entries once, at creation time. */
int paging_reserve_tables(uint32_t start, uint32_t end);

uint32_t paging_kernel_directory(void);        /* physical address of the kernel's directory */

/* ---- per-process address spaces ---- */

/* New directory containing a copy of all kernel mappings. Returns 0 if out of memory. */
uint32_t paging_create_address_space(void);

/* Free every user page, user page table, and the directory itself.
 * Never call this on the directory that is currently loaded in CR3. */
void paging_destroy_address_space(uint32_t pd_phys);

/* Map a page in an arbitrary address space. Returns 0, or -1 if out of memory. */
int paging_map_page_in(uint32_t pd_phys, uint32_t virt, uint32_t phys, uint32_t flags);

/* Load a directory into CR3 (does nothing if it's already loaded). */
void paging_switch(uint32_t pd_phys);

/* Is [addr, addr+len) entirely inside the user range and mapped user-accessible
 * (and writable, if `write`) in this address space? Returns 1 if so. */
int paging_user_range_ok(uint32_t pd_phys, uint32_t addr, uint32_t len, int write);

#endif