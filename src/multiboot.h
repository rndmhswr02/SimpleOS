#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_FLAG_MEM   (1u << 0)   /* mem_lower / mem_upper are valid */
#define MULTIBOOT_FLAG_MMAP  (1u << 6)   /* mmap_* fields are valid */

/* Multiboot 1 information structure (only the fields we need) */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;      /* KiB below 1 MiB */
    uint32_t mem_upper;      /* KiB above 1 MiB */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;    /* total size of the memory map in bytes */
    uint32_t mmap_addr;      /* physical address of the first entry */
} __attribute__((packed));

/* One memory map entry. `size` does NOT include the size field itself. */
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;           /* 1 = available */
} __attribute__((packed));

#define MULTIBOOT_MEMORY_AVAILABLE 1

#endif