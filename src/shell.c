#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "kheap.h"
#include "kprintf.h"
#include "kstring.h"
#include "memops.h"
#include "pmm.h"
#include "sched.h"
#include "timer.h"
#include "vga.h"
#include <stddef.h>
#include <stdint.h>

#define SHELL_LINE_MAX 128
#define SLOTS 16

/* ---- Deliberate crashes ---- */

static void test_div0(void) {
    volatile int a = 1;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}

static void test_gpf(void) {
    __asm__ volatile ("mov $0x28, %%ax\n\tmov %%ax, %%ds" : : : "ax");
}

static void test_ud(void) {
    __asm__ volatile ("ud2");
}

static void test_int3(void) {
    __asm__ volatile ("int3");
}

static void test_pf(void) {
    volatile uint32_t *p = (volatile uint32_t *)0xDEADBEEF;
    *p = 1;
}

static void test_null(void) {
    volatile uintptr_t addr = 0;
    volatile uint32_t *p = (volatile uint32_t *)addr;
    uint32_t v = *p;
    (void)v;
}

static void run_test(const char *name) {
    if (kstrcmp(name, "div0") == 0)       test_div0();
    else if (kstrcmp(name, "gpf") == 0)   test_gpf();
    else if (kstrcmp(name, "ud") == 0)    test_ud();
    else if (kstrcmp(name, "int3") == 0)  test_int3();
    else if (kstrcmp(name, "pf") == 0)    test_pf();
    else if (kstrcmp(name, "null") == 0)  test_null();
    else kprintf("Unknown test '%s'. Try: div0, gpf, ud, int3, pf, null\n", name);
}

/* ---- Timer commands ---- */

static void cmd_uptime(void) {
    uint32_t t  = timer_ticks();
    uint32_t hz = timer_hz();
    uint32_t secs = t / hz;
    uint32_t ms   = (t % hz) * 1000 / hz;
    kprintf("Up %u.%03u s (%u ticks at %u Hz)\n", secs, ms, t, hz);
}

static void cmd_sleep(const char *arg) {
    uint32_t ms;
    if (!kparse_uint(arg, &ms)) {
        kprintf("Usage: sleep <milliseconds>\n");
        return;
    }
    kprintf("Sleeping %u ms...\n", ms);
    task_sleep_ms(ms);                        /* other tasks keep running meanwhile */
    kprintf("Done.\n");
}

/* ---- Memory commands ---- */

static void *slots[SLOTS];

static void cmd_meminfo(void) {
    uint32_t ram   = pmm_ram_frames();
    uint32_t freef = pmm_free_frames();
    uint32_t ks = (uint32_t)(uintptr_t)kernel_start;
    uint32_t ke = (uint32_t)(uintptr_t)kernel_end;

    kprintf("Physical memory (4 KiB frames):\n");
    kprintf("  RAM detected  : %u frames (%u KiB)\n", ram, ram << 2);
    kprintf("  Free          : %u frames (%u KiB)\n", freef, freef << 2);
    kprintf("  Used/reserved : %u frames (%u KiB)\n", ram - freef, (ram - freef) << 2);
    kprintf("Kernel image    : %08x - %08x (%u KiB)\n", ks, ke, (ke - ks) >> 10);

    struct kheap_stats st;
    kheap_stats(&st);
    kprintf("Kernel heap     : starts at %08x, %u KiB mapped\n", HEAP_START, st.mapped_bytes >> 10);
    kprintf("  used %u bytes in %u block(s), free %u bytes in %u block(s)\n",
            st.used_bytes, st.used_blocks, st.free_bytes, st.free_blocks);
}

static void cmd_malloc(const char *arg) {
    uint32_t n;
    if (!kparse_uint(arg, &n) || n == 0) {
        kprintf("Usage: malloc <bytes>\n");
        return;
    }

    for (int i = 0; i < SLOTS; i++) {
        if (!slots[i]) {
            void *p = kmalloc(n);
            if (!p) {
                kprintf("Out of memory.\n");
                return;
            }
            memset(p, 0xAA, n);
            slots[i] = p;
            kprintf("slot %d: %p (%u bytes)\n", i, p, n);
            return;
        }
    }
    kprintf("All %d slots are in use. 'free <slot>' one first.\n", SLOTS);
}

static void cmd_free(const char *arg) {
    uint32_t i;
    if (!kparse_uint(arg, &i) || i >= SLOTS || !slots[i]) {
        kprintf("Usage: free <slot>   (the slot must currently hold a block)\n");
        return;
    }
    kfree(slots[i]);
    slots[i] = 0;
    kprintf("Freed slot %u\n", i);
}

/* ---- Task commands ---- */

/* Prints once a second, then exits. Shows sleeping, waking, and task exit. */
static void task_counter(void *arg) {
    uint32_t n = (uint32_t)(uintptr_t)arg;
    for (uint32_t i = 1; i <= n; i++) {
        task_sleep_ms(1000);
        kprintf("[task %u] %u/%u\n", task_current()->id, i, n);
    }
}

/* Never sleeps or yields: only preemption keeps the system alive. */
static void task_busy(void *arg) {
    (void)arg;
    volatile uint32_t x = 0;
    for (;;)
        x++;
}

static void cmd_spawn(const char *arg) {
    struct task *t = 0;

    if (kstrcmp(arg, "busy") == 0) {
        t = task_create("busy", task_busy, 0);
    } else if (kstarts_with(arg, "count ")) {
        uint32_t n;
        if (!kparse_uint(arg + 6, &n) || n == 0) {
            kprintf("Usage: spawn count <seconds>\n");
            return;
        }
        t = task_create("count", task_counter, (void *)(uintptr_t)n);
    } else {
        kprintf("Usage: spawn busy | spawn count <seconds>\n");
        return;
    }

    if (!t) {
        kprintf("spawn: out of memory\n");
        return;
    }
    kprintf("Started task %u (%s)\n", t->id, t->name);
}

static void cmd_kill(const char *arg) {
    uint32_t id;
    if (!kparse_uint(arg, &id)) {
        kprintf("Usage: kill <task id>   (see 'ps')\n");
        return;
    }

    switch (task_kill(id)) {
    case 0:  kprintf("Killed task %u\n", id); break;
    case -1: kprintf("No task with id %u\n", id); break;
    case -2: kprintf("The kernel and idle tasks can't be killed.\n"); break;
    default: kprintf("Task %u is blocked and can't be killed safely yet.\n", id); break;
    }
}

/* ---- Shell ---- */

void shell_run(void) {
    char line[SHELL_LINE_MAX];

    for (;;) {
        kprint("\nmyos> ");

        size_t len = 0;
        for (;;) {
            char c = keyboard_getchar();

            if (c == '\n') {
                term_putc('\n');
                break;
            } else if (c == '\b') {
                if (len > 0) {
                    len--;
                    term_putc('\b');
                }
            } else if (len < SHELL_LINE_MAX - 1) {
                line[len++] = c;
                term_putc(c);
            }
        }
        line[len] = '\0';

        if (len == 0) {
            continue;
        } else if (kstrcmp(line, "help") == 0) {
            kprintf("Commands: help, clear, about, echo <text>, uptime, sleep <ms>,\n");
            kprintf("          meminfo, memmap, malloc <bytes>, free <slot>, heap,\n");
            kprintf("          ps, spawn <busy|count N>, kill <id>, test <name>, halt\n");
            kprintf("Tests:    div0, gpf, ud, int3, pf, null\n");
        } else if (kstrcmp(line, "clear") == 0) {
            term_clear();
        } else if (kstrcmp(line, "about") == 0) {
            kprintf("MyOS v%d.%d: preemptive multitasking, paging, heap, IRQs.\n", 0, 5);
        } else if (kstarts_with(line, "echo ")) {
            kprintf("%s\n", line + 5);
        } else if (kstrcmp(line, "uptime") == 0) {
            cmd_uptime();
        } else if (kstarts_with(line, "sleep ")) {
            cmd_sleep(line + 6);
        } else if (kstrcmp(line, "meminfo") == 0) {
            cmd_meminfo();
        } else if (kstrcmp(line, "memmap") == 0) {
            pmm_dump_map();
        } else if (kstarts_with(line, "malloc ")) {
            cmd_malloc(line + 7);
        } else if (kstarts_with(line, "free ")) {
            cmd_free(line + 5);
        } else if (kstrcmp(line, "heap") == 0) {
            kheap_dump();
        } else if (kstrcmp(line, "ps") == 0) {
            task_dump();
        } else if (kstarts_with(line, "spawn ")) {
            cmd_spawn(line + 6);
        } else if (kstarts_with(line, "kill ")) {
            cmd_kill(line + 5);
        } else if (kstarts_with(line, "test ")) {
            run_test(line + 5);
        } else if (kstrcmp(line, "halt") == 0) {
            kprintf("Halting CPU. Close the window to exit.\n");
            for (;;) __asm__ volatile ("cli; hlt");
        } else {
            kprintf("Unknown command. Type 'help'.\n");
        }
    }
}