#include <mmu.h>
#include <memlayout.h>
#include <sysregs.h>
#include <peripherals/uart.h>

.section ".text.boot"

.globl _start

_start:
    /* Read cpu id, stop slave cores. */
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbnz    x1, halt

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
    adr     x0, kpgdir

    /* Higher and lower half map to same address. */
    msr     ttbr0_el1, x0
    msr     ttbr1_el1, x0

    ldr     x0, =TCR_VALUE
    msr     tcr_el1, x0

    ldr     x0, =MAIR_VALUE
    msr     mair_el1, x0

   /* Set up stack pointer. */
    ldr     x1, =_start
    msr     spsel, #1
    mov     sp, x1
    isb

    /* Modify System Control Register to enable MMU. */
    mrs     x0, sctlr_el1
    orr     x0, x0, #SCTLR_MMU_ENABLED
    msr     sctlr_el1, x0
    isb

    //adr     x1, vectors
    //msr     vbar_el1, x1
    //isb

    /*
     * Jump to C code, should not return.
     * Why not directly run `b main`?
     */

    ldr     x1, =main
/*
    ldr     x10, =UART_THR(0)
    mov     w11, #52
    strb    w11, [x10]
    dsb     sy
    isb
    b     .
*/
    br      x1

halt:
    wfe
    b       halt
