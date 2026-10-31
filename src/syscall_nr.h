#ifndef SYSCALL_NR_H
#define SYSCALL_NR_H

#define SYS_EXIT    0     /* ebx = exit code */
#define SYS_WRITE   1     /* ebx = fd (1), ecx = buffer, edx = length; returns bytes written or -1 */
#define SYS_GETPID  2     /* returns the process id */
#define SYS_SLEEP   3     /* ebx = milliseconds */
#define SYS_YIELD   4
#define SYS_TICKS   5     /* returns timer ticks since boot */

#endif