#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "sched.h"

/* Create a process from a flat binary that was linked to run at USER_BASE.
 * The image is copied, so the caller's buffer can be reused. Returns NULL on failure. */
struct task *process_spawn(const char *name, const uint8_t *image, uint32_t size);

#endif