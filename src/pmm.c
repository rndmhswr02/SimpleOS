#include "pmm.h"
#include "kprintf.h"
#include "memops.h"
#include "multiboot.h"

#define MAX_FRAMES    (PMM_MAX_PHYS / PAGE_SIZE)     /* 262144 frames = 1 GiB */
#define BITMAP_WORDS  (MAX_FRAMES / 32)              /* 8192 words = 32 KiB   */

static uint32_t bitmap[BITMAP_WORDS];                /* bit = 1: used, 0: free */
static uint32_t ram_frames;
static uint32_t free_frame_count;
static uint32_t highest_phys;
static uint32_t search_hint;                         /* word index to start scanning from */
static const struct multiboot_info *boot_info;

/* ---- bitmap helpers ---- */

static inline void bit_set(uint32_t f)   { bitmap[f / 32] |=  (1u << (f % 32)); }
static inline void bit_clear(uint32_t f) { bitmap[f / 32] &= ~(1u << (f % 32)); }
static inline int  bit_test(uint32_t f)  { return (bitmap[f / 32] >> (f % 32)) & 1; }

/* Mark every whole frame inside [addr, addr+len) as free. Partial frames stay used. */
static void mark_free_range(uint64_t addr, uint64_t len) {
    if (addr >= PMM_MAX_PHYS)
        return;

    uint64_t end = addr + len;
    if (end > PMM_MAX_PHYS)
        end = PMM_MAX_PHYS;

    /* Shifts instead of division: we have no 64-bit divide routine in the kernel. */
    uint32_t first = (uint32_t)((addr + PAGE_SIZE - 1) >> 12);   /* round start up */
    uint32_t last  = (uint32_t)(end >> 12);                      /* round end down */

    for (uint32_t f = first; f < last; f++) {
        if (bit_test(f)) {
            bit_clear(f);
            free_frame_count++;
            ram_frames++;
        }
    }

    if (last > first && last * PAGE_SIZE > highest_phys)
        highest_phys = last * PAGE_SIZE;
}

/* Mark every frame touching [start, end) as used (rounding outward). */
static void reserve_range(uint32_t start, uint32_t end) {
    uint32_t first = start >> 12;
    uint32_t last  = (end + PAGE_SIZE - 1) >> 12;
    if (last > MAX_FRAMES)
        last = MAX_FRAMES;

    for (uint32_t f = first; f < last; f++) {
        if (!bit_test(f)) {
            bit_set(f);
            free_frame_count--;
        }
    }
}

/* ---- public API ---- */

void pmm_init(uint32_t mbi_addr) {
    boot_info = (const struct multiboot_info *)(uintptr_t)mbi_addr;

    memset(bitmap, 0xFF, sizeof(bitmap));            /* start with everything "used" */
    ram_frames = 0;
    free_frame_count = 0;
    highest_phys = 0;
    search_hint = 0;

    if (boot_info->flags & MULTIBOOT_FLAG_MMAP) {
        uint32_t p   = boot_info->mmap_addr;
        uint32_t end = p + boot_info->mmap_length;

        while (p < end) {
            const struct multiboot_mmap_entry *e =
                (const struct multiboot_mmap_entry *)(uintptr_t)p;

            if (e->type == MULTIBOOT_MEMORY_AVAILABLE)
                mark_free_range(e->addr, e->len);

            p += e->size + 4;                         /* size excludes the size field */
        }
    } else if (boot_info->flags & MULTIBOOT_FLAG_MEM) {
        /* No full map: fall back to the simple "KiB above 1 MiB" figure. */
        mark_free_range(0x100000, (uint64_t)boot_info->mem_upper << 10);
    }

    /* Take back everything we must never hand out. */
    reserve_range(0, 0x100000);                                        /* first 1 MiB */
    reserve_range((uint32_t)(uintptr_t)kernel_start,
                  (uint32_t)(uintptr_t)kernel_end);                    /* kernel image */
    reserve_range(mbi_addr, mbi_addr + sizeof(struct multiboot_info)); /* boot info */
    if (boot_info->flags & MULTIBOOT_FLAG_MMAP)
        reserve_range(boot_info->mmap_addr,
                      boot_info->mmap_addr + boot_info->mmap_length);  /* the map itself */
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < BITMAP_WORDS; i++) {
        uint32_t w = (search_hint + i) % BITMAP_WORDS;

        if (bitmap[w] == 0xFFFFFFFFu)
            continue;                                 /* all 32 frames used: skip fast */

        for (uint32_t b = 0; b < 32; b++) {
            if (!(bitmap[w] & (1u << b))) {
                uint32_t frame = w * 32 + b;
                bit_set(frame);
                free_frame_count--;
                search_hint = w;
                return frame * PAGE_SIZE;
            }
        }
    }
    return 0;                                         /* out of memory */
}

void pmm_free_frame(uint32_t phys) {
    uint32_t f = phys >> 12;

    if ((phys & (PAGE_SIZE - 1)) || f >= MAX_FRAMES) {
        kprintf("pmm: bad free of %08x\n", phys);
        return;
    }
    if (!bit_test(f)) {
        kprintf("pmm: double free of %08x\n", phys);
        return;
    }

    bit_clear(f);
    free_frame_count++;
    if (f / 32 < search_hint)
        search_hint = f / 32;
}

uint32_t pmm_ram_frames(void)   { return ram_frames; }
uint32_t pmm_free_frames(void)  { return free_frame_count; }
uint32_t pmm_highest_phys(void) { return highest_phys; }

static const char *type_name(uint32_t type) {
    switch (type) {
    case 1:  return "Available";
    case 2:  return "Reserved";
    case 3:  return "ACPI reclaimable";
    case 4:  return "ACPI NVS";
    case 5:  return "Bad memory";
    default: return "Unknown";
    }
}

void pmm_dump_map(void) {
    if (!(boot_info->flags & MULTIBOOT_FLAG_MMAP)) {
        kprintf("No memory map from the bootloader.\n");
        return;
    }

    uint32_t p   = boot_info->mmap_addr;
    uint32_t end = p + boot_info->mmap_length;

    kprintf("Start             End               Type\n");
    while (p < end) {
        const struct multiboot_mmap_entry *e =
            (const struct multiboot_mmap_entry *)(uintptr_t)p;
        uint64_t last = e->addr + e->len - 1;

        kprintf("%08x%08x    %08x%08x    %s\n",
                (uint32_t)(e->addr >> 32), (uint32_t)e->addr,
                (uint32_t)(last >> 32),    (uint32_t)last,
                type_name(e->type));

        p += e->size + 4;
    }
}