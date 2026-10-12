#include <mmu.h>
#include <memlayout.h>
#include <param.h>
#include <sysregs.h>
#include <sched.h>
#include <peripherals/uart.h>

.section ".boot", "ax"

cpuid_reg .req  x28

// u-bootはすべてのcpuをリトルエンディアン、MMU無効、i/dCache無効の
// 状態で_start関数に制御を渡している

.globl _start

_start:
    // 全コア共通で一度D-Cacheをフラッシュ
    //bl      __asm_flush_dcache_all

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
    /* PC相対（adr）で、新しく.bootセクション内に配置したページテーブルの物理アドレスを100%安全に取得 */
    adr     x0, _boot_pgtbl_start

    /* ページテーブル領域（8KB）をゼロクリア */
    mov     x1, x0
    mov     x2, #1024
.Lclear_loop:
    str     xzr, [x1], #8
    subs    x2, x2, #1
    b.ne    .Lclear_loop

    /* テーブルベースを再セット */
    adr     x0, _boot_pgtbl_start
    add     x1, x0, #0x1000

    /* --- TTBR0用（下位0〜2GB）と TTBR1用（上位0〜2GB）を1つのテーブルに同居させる ---
     * 48ビットアドレス空間（Level 1テーブル）において、
     * - 下位（0〜）のエントリは テーブルのインデックス 0, 1 ...
     * - 上位（0xFFFF0000...）のエントリは テーブルの末尾（インデックス 256, 257 ...）
     *   に位置するため、1つの4KBページの中に両方のエントリを書き込める。
     */

    /* 2. Level 0 テーブルの設定 (0x4000_0000) */
    /* 下位用: Index 0 に Level 1 テーブルへのポインタ(Table Descriptor: 0x3)をセット */
    orr     x2, x1, 0x3
    //ldr     x2, =(0x40001000 | 0x3)
    str     x2, [x0, #0]
    /* 上位用: Index 256 に同じく Level 1 テーブルへのポインタをセット */
    str     x2, [x0, #2048]

    /* エントリ0: 物理 0x0000_0000 〜 0x3FFF_FFFF (1GB分) -> デバイスI/O領域等 */
    ldr     x2, =(0x00000000 | L1_BLOCK_VALID | L1_BLOCK_ATTR | L1_BLOCK_DEVICE)
    str     x2, [x1, #0]        /* Index 0 (物理0〜1GB) */

    /* エントリ1: 物理 0x4000_0000 〜 0x7FFF_FFFF (1GB分) -> DRAM領域（カーネル含む） */
    ldr     x2, =(0x40000000 | L1_BLOCK_VALID | L1_BLOCK_ATTR | L1_BLOCK_NORMAL)
    str     x2, [x1, #8]        /* Index 1 (物理1〜2GB) */

    /* 3. MMU制御レジスタの設定 */
    /* メモリアトリビュート（MAIR）の定義 */
    ldr     x1, =MAIR_VALUE
    msr     mair_el1, x1

    /* 仮想アドレス効果範囲（TCR）の設定 */
    ldr     x1, =TCR_VALUE
    msr     tcr_el1, x1
    isb

    /* ページテーブルの物理ベースアドレスをTTBR0/TTBR1の両方にセット */
    msr     ttbr0_el1, x0
    msr     ttbr1_el1, x0

    // 割込みベクタの登録
    ldr     x0, =vectors
    msr     vbar_el1, x0

    cbnz    cpuid_reg, secondary_core_start

    /* 4. MMUを有効化（SCTLR_EL1のMビットをON） */
    mrs     x1, sctlr_el1
    orr     x1, x1, #1
    msr     sctlr_el1, x1
    isb     /* ここからMMUが有効になり、下位・上位両方のマッピングが有効化 */

// 出力されない
/*
ldr     x10, =UART_THR(0)
//ldr     x12, =KERN_BASE
//sub     x10, x10, x12         // 物理アドレスに変更
mov     w11, #0x63
strb    w11, [x10]            // 'b'を出力
dsb     sy
isb
*/

    /* 5. 仮想アドレスのメイン関数へ「絶対ジャンプ」 */
    ldr     x1, =run_in_va_bsp
    br      x1

secondary_core_start:
    mrs     x1, sctlr_el1
    orr     x1, x1, #1
    msr     sctlr_el1, x1
    isb     /* ここからMMUが有効になり、下位・上位両方のマッピングが有効化 */

    ldr     x1, =run_in_va_ap
    br      x1

/*-------------------------------------------*/

.section ".text"

.macro setup_sp reg1, reg2, reg3
    mov     \reg1, #PGSIZE
    ldr     \reg2, =boot_stacks
    add     \reg3, cpuid_reg, #1
    mul     \reg1, \reg1, \reg3
    add     \reg1, \reg1, \reg2
    msr     spsel, #1
    mov     sp, \reg1
.endm

run_in_va_bsp:
    /* ここからは 0xFFFF_0000_4008_XXXX の仮想アドレス空間で動作 */
    mrs     cpuid_reg, mpidr_el1
    and     cpuid_reg, cpuid_reg, #3

    /* 仮想アドレス空間のカーネルスタックを設定 */
    setup_sp x0, x1, x2

    /* C言語のメイン関数（仮想空間）へ移行 */
    bl      main
1:
    wfi
    b       1b

run_in_va_ap:
    /* ここからは 0xFFFF_0000_4008_XXXX の仮想アドレス空間で動作 */
    mrs     cpuid_reg, mpidr_el1
    and     cpuid_reg, cpuid_reg, #3

    /* 仮想アドレス空間のカーネルスタックを設定 */
    setup_sp x0, x1, x2

    /* C言語のメイン関数（仮想空間）へ移行 */
    mov     x0, cpuid_reg
    bl      secondary_main

2:
    wfi
    bl      2b

/*
ldr     x10, =UART_THR(0)
mov     w11, #52            // '4' : 任意のコード
strb    w11, [x10]
dsb     sy
isb
b     .
*/
// 出力される
/*
ldr     x10, =UART_THR(0)
ldr     x12, =KERN_BASE
sub     x10, x10, x12         // 物理アドレスに変更
mov     w11, #0x61
strb    w11, [x10]            // 'a'を出力
dsb     sy
isb
*/
