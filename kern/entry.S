#include <mmu.h>
#include <memlayout.h>
#include <sysregs.h>
#include <sched.h>
#include <peripherals/uart.h>

.section ".text.boot"

// u-bootはすべてのcpuをリトルエンディアン、MMU無効、i/dCache無効の
// 状態で_start関数に制御を渡している

.globl _start
.globl _start_all

_start:
    // cpu0以外はspinさせる
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbnz    x1, spin_cpux

// cpu0はもちろん、cpu0以外も後ほどcpu0からspinを解除されると
// ここから処理を始める
_start_all:
    /* Get current exception level by CurrentEL[3:2]. */
    mrs     x1, CurrentEL
    and     x1, x1, #0xc
    cmp     x1, #4
    b.eq    el1
    cmp     x1, #8
    b.eq    el2
    b.gt    el3

el3:
    mov     x2, #SCR_VALUE
    msr     scr_el3, x2
    mov     x2, #SPSR_EL3_VALUE
    msr     spsr_el3, x2
    adr     x2, el2
    msr     elr_el3, x2
    eret

el2:
    /* Enable CNTP for EL1. */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3          /* ELOVCTEN | ELOPCTEN */
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr

    /* Enable AArch64 in EL1. */
    mov     x0, #HCR_VALUE      /* AArch64 */
    msr     hcr_el2, x0

    /* Setup SCTLR access. */
    ldr     x2, =SCTLR_VALUE_MMU_DISABLED
    msr     sctlr_el1, x2
    dsb     sy
    isb

    /* Change execution level to EL1. */
    mov     x2, #SPSR_VALUE
    msr     spsr_el2, x2

    adr     x2, el1
    msr     elr_el2, x2
    eret

el1:
    // sp（仮想アドレス）をセットする = (cpuid_reg+1)*4096 + boot_stacks
    // (リニアマッピング: phys 0 --> virt VA_START を想定している)
    mov     x0, #PGSIZE
    ldr     x1, =boot_stacks    //  sched.cで定義
    mrs     x2, mpidr_el1
    and     x2, x2, #3
    add     x2, x2, #1
    mul     x0, x0, x2
    add     x0, x0, x1
    mov     sp, x0

    adr     x0, kpgdir

    /* Higher and lower half map to same address. */
    msr     ttbr0_el1, x0
    msr     ttbr1_el1, x0
    isb

    ldr     x0, =TCR_VALUE
    msr     tcr_el1, x0
    isb

    ldr     x0, =MAIR_VALUE
    msr     mair_el1, x0

   /* Set up stack pointer. */
    //ldr     x1, =_start
    //msr     spsel, #1
    //mov     sp, x1
    //isb

    mrs     x0, mpidr_el1
    and     x0, x0, #3
    b.eq    1f

    ldr     x1, =secondary_main
    b       2f
1:
    ldr     x1, =main

2:
    /* Modify System Control Register to enable MMU. */
    mrs     x2, sctlr_el1
    orr     x2, x2, #SCTLR_MMU_ENABLED
    msr     sctlr_el1, x2
    br      x1

    // cpu0以外はここでスピンする
spin_cpux:
    adr     x5, spin_cpu0
spin:
    wfe
    ldr     x2, [x5, x1, lsl #3]
    cbz     x2, spin
    br      x2

.ltorg

.globl spin_cpu0
.align  3
spin_cpu0:
    .quad 0
.globl spin_cpu1
spin_cpu1:
    .quad 0
.globl spin_cpu2
spin_cpu2:
    .quad 0
.globl spin_cpu3
spin_cpu3:
    .quad 0

