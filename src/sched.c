#include "sched.h"
#include "cpu.h"
#include "kheap.h"
#include "kprintf.h"
#include "timer.h"
#include <stddef.h>
#include <stdint.h>
#include "gdt.h"
#include "paging.h"

#define STACK_SIZE    8192u           /* per-task kernel stack */
#define STACK_CANARY  0xDEADC0DEu     /* written at the far (low) end of each stack */
#define TIME_SLICE    5u              /* ticks per slice: 50 ms at 100 Hz */

extern void switch_context(uint32_t *old_esp, uint32_t new_esp);   /* switch.s */

static struct task *current;          /* the task running right now */
static struct task *main_task;        /* task 0: also the head of the circular list */
static struct task *idle_task;
static struct task *zombie;           /* a dead task waiting to be freed by someone else */
static uint32_t next_id;
static uint32_t slice_left;

/* ---------- helpers ---------- */

static void set_name(struct task *t, const char *name) {
    size_t i = 0;
    for (; name[i] && i < sizeof(t->name) - 1; i++)
        t->name[i] = name[i];
    t->name[i] = '\0';
}

/* Add to the end of the circular list. Caller must have interrupts disabled. */
static void list_append(struct task *t) {
    struct task *tail = main_task;
    while (tail->next != main_task)
        tail = tail->next;
    t->next = main_task;
    tail->next = t;
}

/* Remove from the circular list. `t` must be in it. Interrupts must be disabled. */
static void list_remove(struct task *t) {
    struct task *p = main_task;
    while (p->next != t)
        p = p->next;
    p->next = t->next;
}

static struct task *find_task(uint32_t id) {
    struct task *t = main_task;
    do {
        if (t->id == id)
            return t;
        t = t->next;
    } while (t != main_task);
    return 0;
}

static void destroy_task(struct task *t) {
    list_remove(t);
    if (t->pd)
        paging_destroy_address_space(t->pd);    /* user pages, page tables, directory */
    kfree(t->stack_base);
    kfree(t);
}

/* Free the task that died just before we got the CPU (it can't free itself). */
static void reap(void) {
    if (zombie && zombie != current) {
        struct task *z = zombie;
        zombie = 0;
        destroy_task(z);
    }
}

static void stack_panic(const struct task *t) {
    kprintf("\nSTACK OVERFLOW in task %u (%s)\n", t->id, t->name);
    for (;;)
        __asm__ volatile ("cli; hlt");
}

/* ---------- picking the next task ---------- */

static void wake_sleepers(void) {
    uint32_t now = timer_ticks();
    struct task *t = main_task;
    do {
        /* Signed difference keeps this correct when the tick counter wraps. */
        if (t->state == TASK_SLEEPING && (int32_t)(now - t->wake_tick) >= 0)
            t->state = TASK_READY;
        t = t->next;
    } while (t != main_task);
}

/* Round-robin: first READY task after `current`. Idle only if nothing else can run. */
static struct task *pick_next(void) {
    struct task *t = current;
    do {
        t = t->next;
        if (t != idle_task && t->state == TASK_READY)
            return t;
    } while (t != current);

    /* Nobody else is ready: keep running the current task if it still wants to run. */
    if (current != idle_task && current->state == TASK_RUNNING)
        return current;
    return idle_task;
}

/* The heart of the scheduler. Safe to call from task context or from an IRQ handler. */
static void schedule(void) {
    uint32_t flags = irq_save();

    wake_sleepers();

    struct task *prev = current;
    if (prev->stack_base && *(uint32_t *)prev->stack_base != STACK_CANARY)
        stack_panic(prev);

    struct task *next = pick_next();
    slice_left = TIME_SLICE;

    if (prev->state == TASK_RUNNING && next != prev)
        prev->state = TASK_READY;
    next->state = TASK_RUNNING;

    if (next != prev) {
        current = next;

        /* Ring-3 interrupts and syscalls must land on THIS task's kernel stack. */
        if (next->stack_base)
            tss_set_kernel_stack((uint32_t)(uintptr_t)next->stack_base + STACK_SIZE);

        /* Kernel tasks use the kernel directory; processes use their own. */
        paging_switch(next->pd ? next->pd : paging_kernel_directory());

        switch_context(&prev->esp, next->esp);

        /* We only get here when somebody switches back to us. */
        reap();
    }

    irq_restore(flags);
}

/* ---------- creating tasks ---------- */

/* First code a new task runs. We arrive here from switch_context's `ret`. */
static void task_trampoline(void) {
    reap();                                   /* interrupts are still off here */
    __asm__ volatile ("sti");                 /* the task itself runs with interrupts on */
    current->entry(current->arg);
    task_exit();
}

struct task *task_create(const char *name, void (*entry)(void *), void *arg) {
    struct task *t = kcalloc(1, sizeof(struct task));
    uint8_t *stack = kmalloc(STACK_SIZE);

    if (!t || !stack) {
        kfree(t);                             /* kfree(NULL) is harmless */
        kfree(stack);
        return 0;
    }

    set_name(t, name);
    t->id = next_id++;
    t->state = TASK_READY;
    t->entry = entry;
    t->arg = arg;
    t->stack_base = stack;
    *(uint32_t *)stack = STACK_CANARY;        /* overflow detector at the low end */

    /* Fake the stack of a task that was switched out inside switch_context():
     * popping edi, esi, ebx, ebp and then `ret` lands in task_trampoline. */
    uint32_t *sp = (uint32_t *)(stack + STACK_SIZE);
    *--sp = 0;                                            /* fake return address for the trampoline */
    *--sp = (uint32_t)(uintptr_t)task_trampoline;         /* where `ret` will go */
    *--sp = 0;                                            /* ebp */
    *--sp = 0;                                            /* ebx */
    *--sp = 0;                                            /* esi */
    *--sp = 0;                                            /* edi  <- saved esp */
    t->esp = (uint32_t)(uintptr_t)sp;

    uint32_t flags = irq_save();
    list_append(t);
    irq_restore(flags);
    return t;
}

static void idle_main(void *arg) {
    (void)arg;
    for (;;)
        __asm__ volatile ("hlt");             /* sleep until the next interrupt */
}

void sched_init(void) {
    /* The code running right now (kernel_main, then the shell) becomes task 0. */
    main_task = kcalloc(1, sizeof(struct task));
    set_name(main_task, "kernel");
    main_task->id = next_id++;
    main_task->state = TASK_RUNNING;
    main_task->next = main_task;              /* a list of one */
    current = main_task;

    idle_task = task_create("idle", idle_main, 0);
}

/* ---------- public API ---------- */

struct task *task_current(void) {
    return current;
}

void task_yield(void) {
    schedule();
}

void task_exit(void) {
    irq_save();                               /* never restored: we are not coming back */
    current->state = TASK_DEAD;
    zombie = current;
    schedule();

    for (;;)                                  /* unreachable: a dead task is never scheduled */
        __asm__ volatile ("hlt");
}

void task_sleep_ms(uint32_t ms) {
    uint32_t ticks = (ms * timer_hz() + 999) / 1000;     /* round up */

    uint32_t flags = irq_save();
    current->wake_tick = timer_ticks() + ticks;
    current->state = TASK_SLEEPING;
    schedule();
    irq_restore(flags);
}

void task_block(void) {
    uint32_t flags = irq_save();
    current->state = TASK_BLOCKED;
    schedule();
    irq_restore(flags);
}

void task_wake(struct task *t) {
    uint32_t flags = irq_save();
    if (t->state == TASK_BLOCKED)
        t->state = TASK_READY;
    irq_restore(flags);
}

int task_kill(uint32_t id) {
    uint32_t flags = irq_save();
    int rc;

    struct task *t = find_task(id);
    if (!t) {
        rc = -1;
    } else if (t == main_task || t == idle_task) {
        rc = -2;
    } else if (t == current) {
        task_exit();                          /* does not return */
    } else if (t->state == TASK_BLOCKED) {
        rc = -3;                              /* something may still hold a pointer to it */
    } else {
        destroy_task(t);                      /* it isn't running, so its stack is free to go */
        rc = 0;
    }

    irq_restore(flags);
    return rc;
}

/* ---------- preemption ---------- */

/* Called from the timer interrupt on every tick. */
static void sched_tick(void) {
    current->cpu_ticks++;

    /* A task keeps the CPU for TIME_SLICE ticks. The idle task is always
     * rescheduled so it notices newly woken tasks within one tick. */
    if (current != idle_task && slice_left > 1) {
        slice_left--;
        return;
    }
    schedule();
}

void sched_start_preemption(void) {
    timer_set_hook(sched_tick);
}

/* ---------- ps ---------- */

static const char *state_name(enum task_state s) {
    switch (s) {
    case TASK_READY:    return "READY";
    case TASK_RUNNING:  return "RUNNING";
    case TASK_SLEEPING: return "SLEEPING";
    case TASK_BLOCKED:  return "BLOCKED";
    default:            return "DEAD";
    }
}

static void print_padded(const char *s, uint32_t width) {
    uint32_t n = 0;
    while (s[n])
        n++;
    kprintf("%s", s);
    for (; n < width; n++)
        kprintf(" ");
}

void task_dump(void) {
    uint32_t flags = irq_save();

    kprintf("ID  NAME        STATE     MODE    CPU TICKS\n");
    struct task *t = main_task;
    do {
        kprintf("%2u  ", t->id);
        print_padded(t->name, 12);
        print_padded(state_name(t->state), 10);
        print_padded(t->pd ? "user" : "kernel", 8);
        kprintf("%u\n", t->cpu_ticks);
        t = t->next;
    } while (t != main_task);

    irq_restore(flags);
}