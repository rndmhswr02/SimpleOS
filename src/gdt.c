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
    uint16_t limit;          /* size of table in bytes, minus 1 */
    uint32_t base;           /* linear address of the table */
} __attribute__((packed));

#define GDT_ENTRIES 3

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdtp;

/* Implemented in gdt_flush.s: loads the GDT and reloads all segment registers */
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

    /*  index  base  limit    access  flags
     *  access 0x9A = present, ring 0, code, executable, readable
     *  access 0x92 = present, ring 0, data, writable
     *  flags  0xC0 = 4 KiB granularity, 32-bit segment
     *  limit 0xFFFFF * 4 KiB = 4 GiB (flat model)
     */
    gdt_set(0, 0, 0,       0x00, 0x00);   /* null descriptor */
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xC0);   /* kernel code, selector 0x08 */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xC0);   /* kernel data, selector 0x10 */

    gdt_flush((uint32_t)(uintptr_t)&gdtp);
}