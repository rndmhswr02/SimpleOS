#include "paging.h"
#include "kprintf.h"
#include "memops.h"
#include "pmm.h"

#define ENTRIES_PER_TABLE 1024u

static uint32_t *page_directory;       /* physical == virtual thanks to the identity map */

static inline void invlpg(uint32_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static void out_of_memory(void) {
    kprintf("paging: out of physical memory while building page tables\n");
    for (;;)
        __asm__ volatile ("cli; hlt");
}

uint32_t paging_init(void) {
    uint32_t limit  = pmm_highest_phys();
    uint32_t tables = (limit + 0x3FFFFFu) >> 22;      /* number of 4 MiB chunks to map */
    if (tables == 0)
        tables = 1;

    page_directory = (uint32_t *)(uintptr_t)pmm_alloc_frame();
    if (!page_directory)
        out_of_memory();
    memset(page_directory, 0, PAGE_SIZE);

    /* Identity-map [0, tables * 4 MiB) with one page table per 4 MiB. */
    for (uint32_t t = 0; t < tables; t++) {
        uint32_t *pt = (uint32_t *)(uintptr_t)pmm_alloc_frame();
        if (!pt)
            out_of_memory();

        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; i++) {
            uint32_t phys = (t << 22) | (i << 12);
            pt[i] = phys | PAGE_PRESENT | PAGE_WRITE;
        }
        page_directory[t] = (uint32_t)(uintptr_t)pt | PAGE_PRESENT | PAGE_WRITE;
    }

    /* Guard page: unmap virtual page 0 so NULL dereferences fault. */
    uint32_t *pt0 = (uint32_t *)(uintptr_t)(page_directory[0] & 0xFFFFF000u);
    pt0[0] = 0;

    /* Load CR3, then set CR0.PG (paging) and CR0.WP (kernel honors read-only pages). */
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)(uintptr_t)page_directory) : "memory");

    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u | 0x00010000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");

    return tables * 4;
}

int paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t *pt;

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        /* No page table covers this 4 MiB region yet: allocate one. */
        uint32_t frame = pmm_alloc_frame();
        if (!frame)
            return -1;

        pt = (uint32_t *)(uintptr_t)frame;
        memset(pt, 0, PAGE_SIZE);
        page_directory[pd_index] = frame | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    } else {
        pt = (uint32_t *)(uintptr_t)(page_directory[pd_index] & 0xFFFFF000u);
    }

    pt[pt_index] = (phys & 0xFFFFF000u) | (flags & 0xFFFu) | PAGE_PRESENT;
    invlpg(virt);
    return 0;
}

uint32_t paging_unmap_page(uint32_t virt) {
    uint32_t pde = page_directory[virt >> 22];
    if (!(pde & PAGE_PRESENT))
        return 0;

    uint32_t *pt  = (uint32_t *)(uintptr_t)(pde & 0xFFFFF000u);
    uint32_t index = (virt >> 12) & 0x3FF;
    uint32_t pte  = pt[index];
    if (!(pte & PAGE_PRESENT))
        return 0;

    pt[index] = 0;
    invlpg(virt);
    return pte & 0xFFFFF000u;
}

uint32_t paging_get_phys(uint32_t virt) {
    uint32_t pde = page_directory[virt >> 22];
    if (!(pde & PAGE_PRESENT))
        return 0;

    uint32_t *pt = (uint32_t *)(uintptr_t)(pde & 0xFFFFF000u);
    uint32_t pte = pt[(virt >> 12) & 0x3FF];
    if (!(pte & PAGE_PRESENT))
        return 0;

    return (pte & 0xFFFFF000u) | (virt & 0xFFFu);
}