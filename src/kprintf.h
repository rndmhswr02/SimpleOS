#ifndef KPRINTF_H
#define KPRINTF_H

/*
 * Minimal printf. Supported:
 *   %d  signed decimal      %u  unsigned decimal
 *   %x  hex (lowercase)     %p  pointer (0x + 8 hex digits)
 *   %c  character           %s  string
 *   %%  literal percent
 * Optional zero flag and width, e.g. %08x or %5d.
 */
void kprintf(const char *fmt, ...);

#endif