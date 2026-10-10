#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE      4096u
#define PMM_MAX_PHYS   0x40000000u     /* we manage at most the first 1 GiB of RAM */

/* Linker-provided symbols marking the kernel image */
extern char kernel_start[];
extern char kernel_end[];

/* Parse the Multiboot memory map and build the free-frame bitmap. */
void pmm_init(uint32_t mbi_addr);

/* Allocate one 4 KiB frame. Returns its physical address, or 0 if out of memory. */
uint32_t pmm_alloc_frame(void);

/* Return a frame to the allocator. */
void pmm_free_frame(uint32_t phys);

uint32_t pmm_ram_frames(void);       /* frames of usable RAM found in the map */
uint32_t pmm_free_frames(void);      /* frames currently free */
uint32_t pmm_highest_phys(void);     /* end of the highest usable RAM (bytes, capped at 1 GiB) */

/* Print the bootloader's memory map. */
void pmm_dump_map(void);

#endif