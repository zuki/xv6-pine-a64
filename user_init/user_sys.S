# for user threads (compiled as part of the kernel code)  to make syscalls

#include "linux/syscall.h"

.globl call_sys_write
  call_sys_write:
    mov x8, #SYS_write
    svc #0
    ret

.globl call_sys_exec
  call_sys_exec:
    mov x8, #SYS_execve
    svc #0
    ret

.globl call_sys_open
  call_sys_open:
    mov x8, #SYS_openat
    svc #0
    ret


.globl call_sys_mknod
  call_sys_mknod:
    mov x8, #SYS_mknodat
    svc #0
    ret

.globl call_sys_dup
    call_sys_dup:
    mov x8, #SYS_dup
    svc #0
    ret

.globl call_sys_exit
    call_sys_exit:
    mov x8, #SYS_exit
    svc #0
    ret
