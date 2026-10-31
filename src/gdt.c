#include "gdt.h"
#include <stdint.h>

/* One 8-byte segment descriptor */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;    /* high nibble: flags, low nibble: limit bits 16-19 */
    uint8_t  base_high;
} __attribute__((packed));

/* The pointer structure handed to the `lgdt` instruction */
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* 32-bit Task State Segment. We only use ss0/esp0 (the kernel stack for ring 3 -> 0). */
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

#define GDT_ENTRIES 6

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdtp;
static struct tss_entry tss;

/* Implemented in gdt_flush.s */
extern void gdt_flush(uint32_t gdt_ptr_address);

static void gdt_set(int i, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t flags) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[i].access      = access;
}

void gdt_init(void) {
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint32_t)(uintptr_t)&gdt;

    /* access: 0x9A/0x92 = ring 0 code/data;  0xFA/0xF2 = ring 3 code/data
     * flags 0xC0: 4 KiB granularity, 32-bit segment */
    gdt_set(0, 0, 0,       0x00, 0x00);   /* null descriptor */
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xC0);   /* 0x08 kernel code */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xC0);   /* 0x10 kernel data */
    gdt_set(3, 0, 0xFFFFF, 0xFA, 0xC0);   /* 0x18 user code (use as 0x1B) */
    gdt_set(4, 0, 0xFFFFF, 0xF2, 0xC0);   /* 0x20 user data (use as 0x23) */

    /* TSS: access 0x89 = present, 32-bit TSS (available); byte granularity */
    tss.ss0 = SEL_KERNEL_DATA;
    tss.esp0 = 0;                          /* set by the scheduler before ring 3 runs */
    tss.iomap_base = sizeof(tss);          /* offset beyond the limit: no I/O bitmap, so
                                              ring 3 can't touch any I/O port */
    gdt_set(5, (uint32_t)(uintptr_t)&tss, sizeof(tss) - 1, 0x89, 0x00);   /* 0x28 */

    gdt_flush((uint32_t)(uintptr_t)&gdtp);

    /* Load the task register with the TSS selector. */
    __asm__ volatile ("ltr %%ax" : : "a"((uint16_t)0x28));
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}