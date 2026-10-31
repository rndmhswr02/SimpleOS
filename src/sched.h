#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

enum task_state {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_DEAD
};

struct task {
    uint32_t esp;                 /* saved kernel stack pointer while not running */
    uint32_t id;
    char name[16];
    enum task_state state;
    uint32_t wake_tick;           /* for TASK_SLEEPING */
    uint32_t cpu_ticks;           /* timer ticks during which this task was running */
    void *stack_base;             /* kmalloc'd stack (NULL for the boot task) */
    void (*entry)(void *);
    void *arg;
    struct task *next;   
    uint32_t pd;                  /* physical address of this task's page directory (0 = the kernel's) */         /* circular list of all tasks */
};

/* Turn the code that is running right now into task 0 ("kernel") and create the idle task.
 * Call after kheap_init(). */
void sched_init(void);

/* Install the timer hook that makes scheduling preemptive. */
void sched_start_preemption(void);

/* Create a task that will run entry(arg). Returns NULL if out of memory. */
struct task *task_create(const char *name, void (*entry)(void *), void *arg);

void task_yield(void);                          /* give up the CPU voluntarily */
void task_exit(void) __attribute__((noreturn)); /* end the calling task */
void task_sleep_ms(uint32_t ms);                /* sleep without using CPU */
void task_block(void);                          /* sleep until task_wake() is called on us */
void task_wake(struct task *t);                 /* make a BLOCKED task runnable (IRQ-safe) */
struct task *task_current(void);

/* Kill another task. Returns 0 on success, -1 not found, -2 protected (kernel/idle),
 * -3 task is blocked (refused, see the exercises). */
int task_kill(uint32_t id);

void task_dump(void);                           /* print the task table (like `ps`) */

#endif