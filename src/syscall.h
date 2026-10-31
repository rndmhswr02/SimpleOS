#ifndef SYSCALL_H
#define SYSCALL_H

#include "isr.h"
#include "syscall_nr.h"

/* Called from isr_handler() for int 0x80. Reads the call number and arguments from
 * the saved registers and stores the result in r->eax (which popal restores to user mode). */
void syscall_dispatch(struct registers *r);

#endif