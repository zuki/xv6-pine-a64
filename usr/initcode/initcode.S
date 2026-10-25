#include <linux/syscall.h>

.globl _start
_start:
    mov   x8, #SYS_execve
    adr   x0, init
    mov   x1, argv
    mov   x2, envp
    svc   #0

init:
    .string "/bin/init\0"

path:
    .string "PATH=/bin\0"

.align 4
argv:
    .dword init, 0

.align 4
envp:
    .dword path, 0

.align 4
_end:

