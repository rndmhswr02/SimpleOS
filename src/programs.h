#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "sched.h"

/* Start the built-in program with this name as a new process. NULL if unknown or out of memory. */
struct task *program_run(const char *name);

/* Print the names of all built-in programs. */
void program_list(void);

#endif