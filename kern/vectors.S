#include <memlayout.h>

#define ventry .align 7; b alltraps

#define verror(type) .align 7; mov x0, #(type); b irq_error

.globl vectors

.align 12
vectors:
el1_sp0:
    verror(0)     // syncronous
    verror(1)     // irq
    verror(2)     // fiq
    verror(3)     // serror

.align 7
el1_spx:
    ventry
    ventry
    verror(6)
    verror(7)

.align 7
el0_aarch64:
    ventry        // syncronous : syscall
    ventry        // irq  : uart, timer, etc
    verror(10)
    verror(11)

.align 7
el0_aarch32:
    verror(12)
    verror(13)
    verror(14)
    verror(15)


