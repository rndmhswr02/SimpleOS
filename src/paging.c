#include "paging.h"
#include "kprintf.h"
#include "memops.h"
#include "pmm.h"

#define ENTRIES_PER_TABLE 1024u

static uint32_t *page_directory;       /* the kernel's directory (physical == virtual) */
static uint32_t current_pd;            /* what is loaded in CR3 right now */

static inline void invlpg(uint32_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static void out_of_memory(void) {
    kprintf("paging: out of physical memory while building page tables\n");
    for (;;)
        __asm__ volatile ("cli; hlt");
}

static inline uint32_t *as_table(uint32_t phys) {
    return (uint32_t *)(uintptr_t)(phys & 0xFFFFF000u);
}

uint32_t paging_init(void) {
    uint32_t limit  = pmm_highest_phys();
    uint32_t tables = (limit + 0x3FFFFFu) >> 22;      /* number of 4 MiB chunks to map */
    if (tables == 0)
        tables = 1;

    page_directory = as_table(pmm_alloc_frame());
    if (!page_directory)
        out_of_memory();
    memset(page_directory, 0, PAGE_SIZE);

    /* Identity-map [0, tables * 4 MiB). Note: no PAGE_USER, so ring 3 can't touch it. */
    for (uint32_t t = 0; t < tables; t++) {
        uint32_t *pt = as_table(pmm_alloc_frame());
        if (!pt)
            out_of_memory();

        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; i++) {
            uint32_t phys = (t << 22) | (i << 12);
            pt[i] = phys | PAGE_PRESENT | PAGE_WRITE;
        }
        page_directory[t] = (uint32_t)(uintptr_t)pt | PAGE_PRESENT | PAGE_WRITE;
    }

    /* Guard page: unmap virtual page 0 so NULL dereferences fault. */
    as_table(page_directory[0])[0] = 0;

    /* Load CR3, then set CR0.PG (paging) and CR0.WP (kernel honors read-only pages). */
    current_pd = (uint32_t)(uintptr_t)page_directory;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(current_pd) : "memory");

    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u | 0x00010000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");

    return tables * 4;
}

uint32_t paging_kernel_directory(void) {
    return (uint32_t)(uintptr_t)page_directory;
}

void paging_switch(uint32_t pd_phys) {
    if (pd_phys == current_pd)
        return;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys) : "memory");   /* also flushes the TLB */
    current_pd = pd_phys;
}

int paging_map_page_in(uint32_t pd_phys, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t *pd = as_table(pd_phys);
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t *pt;

    if (!(pd[pd_index] & PAGE_PRESENT)) {
        /* No page table covers this 4 MiB region yet: allocate one. */
        uint32_t frame = pmm_alloc_frame();
        if (!frame)
            return -1;

        pt = as_table(frame);
        memset(pt, 0, PAGE_SIZE);
        pd[pd_index] = frame | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    } else {
        pt = as_table(pd[pd_index]);
        if (flags & PAGE_USER)
            pd[pd_index] |= PAGE_USER;     /* effective access = directory AND table entry */
    }

    pt[pt_index] = (phys & 0xFFFFF000u) | (flags & 0xFFFu) | PAGE_PRESENT;
    invlpg(virt);
    return 0;
}

int paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    return paging_map_page_in(paging_kernel_directory(), virt, phys, flags);
}

uint32_t paging_unmap_page(uint32_t virt) {
    uint32_t pde = page_directory[virt >> 22];
    if (!(pde & PAGE_PRESENT))
        return 0;

    uint32_t *pt   = as_table(pde);
    uint32_t index = (virt >> 12) & 0x3FF;
    uint32_t pte   = pt[index];
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

    uint32_t pte = as_table(pde)[(virt >> 12) & 0x3FF];
    if (!(pte & PAGE_PRESENT))
        return 0;

    return (pte & 0xFFFFF000u) | (virt & 0xFFFu);
}

int paging_reserve_tables(uint32_t start, uint32_t end) {
    for (uint32_t v = start & 0xFFC00000u; v < end; v += 0x400000u) {
        uint32_t index = v >> 22;
        if (!(page_directory[index] & PAGE_PRESENT)) {
            uint32_t frame = pmm_alloc_frame();
            if (!frame)
                return -1;
            memset(as_table(frame), 0, PAGE_SIZE);
            page_directory[index] = frame | PAGE_PRESENT | PAGE_WRITE;   /* kernel-only */
        }
    }
    return 0;
}

uint32_t paging_create_address_space(void) {
    uint32_t frame = pmm_alloc_frame();
    if (!frame)
        return 0;

    /* Start from a copy of the kernel's directory: every process sees the kernel. */
    memcpy(as_table(frame), page_directory, PAGE_SIZE);
    return frame;
}

void paging_destroy_address_space(uint32_t pd_phys) {
    uint32_t *pd = as_table(pd_phys);

    for (uint32_t i = 0; i < ENTRIES_PER_TABLE; i++) {
        /* Only directory entries with the User bit belong to this process.
         * The kernel's entries (copied at creation) have no User bit and are shared. */
        if ((pd[i] & PAGE_PRESENT) && (pd[i] & PAGE_USER)) {
            uint32_t *pt = as_table(pd[i]);
            for (uint32_t j = 0; j < ENTRIES_PER_TABLE; j++) {
                if (pt[j] & PAGE_PRESENT)
                    pmm_free_frame(pt[j] & 0xFFFFF000u);
            }
            pmm_free_frame((uint32_t)(uintptr_t)pt);
        }
    }
    pmm_free_frame(pd_phys);
}

int paging_user_range_ok(uint32_t pd_phys, uint32_t addr, uint32_t len, int write) {
    if (len == 0)
        return 1;

    uint32_t end = addr + len;
    if (end < addr)                                  /* wrapped around */
        return 0;
    if (addr < USER_BASE || end > USER_LIMIT)
        return 0;

    uint32_t *pd = as_table(pd_phys);
    uint32_t need = PAGE_PRESENT | PAGE_USER | (write ? PAGE_WRITE : 0);

    for (uint32_t page = addr & 0xFFFFF000u; page < end; page += PAGE_SIZE) {
        uint32_t pde = pd[page >> 22];
        if ((pde & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER))
            return 0;

        uint32_t pte = as_table(pde)[(page >> 12) & 0x3FF];
        if ((pte & need) != need)
            return 0;
    }
    return 1;
}