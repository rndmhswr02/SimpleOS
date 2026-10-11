.att_syntax prefix

.section .text
.global switch_context

# void switch_context(uint32_t *old_esp, uint32_t new_esp)
#
# Saves the callee-saved registers on the current stack, stores the stack pointer
# in *old_esp, switches to new_esp, restores that stack's registers, and returns
# into whatever the new task was doing.
switch_context:
    movl 4(%esp), %eax        # eax = old_esp (pointer to where we store our esp)
    movl 8(%esp), %edx        # edx = new_esp (stack to switch to)

    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl %esp, (%eax)         # *old_esp = esp
    movl %edx, %esp           # switch stacks

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret                       # return into the new task