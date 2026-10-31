#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define SEL_KERNEL_CODE 0x08
#define SEL_KERNEL_DATA 0x10
#define SEL_USER_CODE   0x1B     /* index 3, RPL 3 */
#define SEL_USER_DATA   0x23     /* index 4, RPL 3 */

void gdt_init(void);

/* Tell the CPU which stack to use when ring-3 code enters the kernel. */
void tss_set_kernel_stack(uint32_t esp0);

#endif