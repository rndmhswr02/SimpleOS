#include "programs.h"
#include "kprintf.h"
#include "kstring.h"
#include "process.h"
#include <stdint.h>

/* Defined in userprogs.s */
extern const uint8_t prog_hello_start[],      prog_hello_end[];
extern const uint8_t prog_spin_start[],       prog_spin_end[];
extern const uint8_t prog_crash_mem_start[],  prog_crash_mem_end[];
extern const uint8_t prog_crash_priv_start[], prog_crash_priv_end[];
extern const uint8_t prog_badptr_start[],     prog_badptr_end[];

struct program {
    const char *name;
    const char *description;
    const uint8_t *start;
    const uint8_t *end;
};

static const struct program programs[] = {
    { "hello",      "prints via system calls, sleeps, exits",  prog_hello_start,      prog_hello_end },
    { "spin",       "infinite loop in ring 3 (use 'kill')",    prog_spin_start,       prog_spin_end },
    { "crash_mem",  "writes to kernel memory: gets killed",    prog_crash_mem_start,  prog_crash_mem_end },
    { "crash_priv", "executes 'cli' in ring 3: gets killed",   prog_crash_priv_start, prog_crash_priv_end },
    { "badptr",     "passes a kernel pointer to write()",      prog_badptr_start,     prog_badptr_end },
};

#define PROGRAM_COUNT (sizeof(programs) / sizeof(programs[0]))

struct task *program_run(const char *name) {
    for (uint32_t i = 0; i < PROGRAM_COUNT; i++) {
        if (kstrcmp(name, programs[i].name) == 0) {
            uint32_t size = (uint32_t)(programs[i].end - programs[i].start);
            return process_spawn(programs[i].name, programs[i].start, size);
        }
    }
    return 0;
}

void program_list(void) {
    for (uint32_t i = 0; i < PROGRAM_COUNT; i++) {
        uint32_t size = (uint32_t)(programs[i].end - programs[i].start);
        kprintf("  %s (%u bytes): %s\n", programs[i].name, size, programs[i].description);
    }
}