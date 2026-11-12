#include "linux/syscall.h"

.globl _start
_start:
    mov   x8, #SYS_execve
    adr   x0, init
    mov   x1, 0
    mov   x2, 0
    svc   #0

init:
    .string "/bin/init\0"

.align 4
_end:

