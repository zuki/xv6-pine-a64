#include <mmu.h>
#include <memlayout.h>
#include <param.h>
#include <sysregs.h>
#include <sched.h>
#include <peripherals/uart.h>

.section ".text.boot"

cpuid_reg .req  x28

// u-bootはすべてのcpuをリトルエンディアン、MMU無効、i/dCache無効の
// 状態で_start関数に制御を渡している

.globl _start
.globl mp_start

_start:
    // 全コア共通で一度D-Cacheをフラッシュ
    bl      __asm_flush_dcache_all

    mrs     cpuid_reg, mpidr_el1
    and     cpuid_reg, cpuid_reg, #3

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
    cbnz    cpuid_reg, setup_sp

    // 以下はcpu0だけが実行。bss領域のクリア。
    // bss_begin/end はリンクアドレス (カーネル仮想アドレス). 物理アドレスに変換する
    // これらのアドレスはリンカスクリプトで（少なくとも）8バイトアラインされている
    ldr     x0, =bss_begin
    ldr     x1, =bss_end
    ldr     x2, =VA_START
    sub     x0, x0, x2
    sub     x1, x1, x2
    sub     x1, x1, x0
    bl      memzero_aligned

setup_sp:
    // sp（仮想アドレス）をセットする = (cpuid_reg+1)*4096 + boot_stacks
    // (リニアマッピング: phys 0 --> virt VA_START を想定している)
    mov     x0, #PGSIZE
    ldr     x1, =boot_stacks    //  sched.cで定義
    add     x2, cpuid_reg, #1
    mul     x0, x0, x2
    add     x0, x0, x1
    msr     spsel, #1
    mov     sp, x0


/*
ldr     x10, =UART_THR(0)
mov     w11, w28              // '4' : 任意のコード
add     w11, w11, #0x30
strb    w11, [x10]
dsb     sy
isb
*/
    // cpu1-3をspinさせる
    cbnz    cpuid_reg, secondary_core_setup

    /* Higher and lower half map to same address. */
    adr     x0, kpgdir
    msr     ttbr0_el1, x0
    msr     ttbr1_el1, x0
    isb

    ldr     x0, =TCR_VALUE
    msr     tcr_el1, x0
    isb

    ldr     x0, =MAIR_VALUE
    msr     mair_el1, x0

    // 割込みベクタの登録
    ldr     x0, =vectors
    msr     vbar_el1, x0

    ldr     x2, =main
    ldr     x0, =SCTLR_VALUE
    msr     sctlr_el1, x0
    br      x2

/*
ldr     x10, =UART_THR(0)
mov     w11, #52            // '4' : 任意のコード
strb    w11, [x10]
dsb     sy
isb
b     .
*/

// cpu0以外の分岐先
secondary_core_asm:
    mov     x0, cpuid_reg
    bl      secondary_main

// ーーー コア1〜3（AP）の処理 ーーー
secondary_core_setup:
    // BSPと同じMMU設定をAPにも適用
    adr     x1, kpgdir
    msr     ttbr0_el1, x1
    msr     ttbr1_el1, x1
    isb

    ldr     x1, =TCR_VALUE
    msr     tcr_el1, x1
    ldr     x1, =MAIR_VALUE
    msr     mair_el1, x1
    isb

    ldr     x1, =vectors
    msr     vbar_el1, x1
    isb

    ldr     x2, =secondary_core_asm

    // MMUを有効化して、secondary_core_asm へ安全に br する
    ldr     x1, =SCTLR_VALUE
    msr     sctlr_el1, x1
    isb

    br      x2

.ltorg

.align  3
.globl spin_core0
spin_core0:
    .quad 0
.globl spin_core1
spin_core1:
    .quad 0
.globl spin_core2
spin_core2:
    .quad 0
.globl spin_core3
spin_core3:
    .quad 0

