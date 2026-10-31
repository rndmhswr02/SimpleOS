.att_syntax prefix
.section .rodata

# Each program's flat binary (built by build.ps1 into build/user/) is pasted in here.

.balign 16
.global prog_hello_start
.global prog_hello_end
prog_hello_start:
    .incbin "build/user/hello.bin"
prog_hello_end:

.balign 16
.global prog_spin_start
.global prog_spin_end
prog_spin_start:
    .incbin "build/user/spin.bin"
prog_spin_end:

.balign 16
.global prog_crash_mem_start
.global prog_crash_mem_end
prog_crash_mem_start:
    .incbin "build/user/crash_mem.bin"
prog_crash_mem_end:

.balign 16
.global prog_crash_priv_start
.global prog_crash_priv_end
prog_crash_priv_start:
    .incbin "build/user/crash_priv.bin"
prog_crash_priv_end:

.balign 16
.global prog_badptr_start
.global prog_badptr_end
prog_badptr_start:
    .incbin "build/user/badptr.bin"
prog_badptr_end: