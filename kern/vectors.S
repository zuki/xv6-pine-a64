#include <types.h>
#include <linux/syscall.h>
#include <param.h>
#include <mmu.h>
#include <sched.h>
#include <vectors.h>
#include <sysregs.h>
#include <irq.h>

// ハンドラのない割り込みのエラーハンドラ
.macro handle_invalid_entry el, type
    // カーネルはいずれにしてもハングアップしてしまうので
    // カーネルスタックを使うことにする
    mrs   x27, sctlr_el1        // x27/x28には重要なデータはないと仮定
    orr   x27, x27, #1          // sctlr_el1 bit0: MMUはオンかオフか. cf sysregs.h
    cmp   x27, #0
    b.eq  99f
                                // MMUはオン、仮想アドレスを使う
    ldr   x27, =boot_stacks     // boot_stacks[NCPU][4096]
    add   x27, x27, PAGE_SIZE
    mov   x28, #0
    b 100f
99:                             // MMUはオフ、物理アドレスを使う
    //mov x27, #LOW_MEMORY
    //mov x28, #0
    ldr   x27, =boot_stacks
    add   x27, x27, PAGE_SIZE
    mov   x28, #VA_START
100:
    //add sp, x27, x28
    sub   sp, x27, x28
    kernel_entry \el
    mov   x0, #\type
    mrs   x1, esr_el1
    mrs   x2, elr_el1
    mrs   x3, far_el1
    bl    show_invalid_entry_message      // エラー表示: irq.c
    msr   daifset, #0b0010              // 割り込みを無効にする
    b     err_hang
.endm

.macro    ventry    label
.align    7
    b     \label
.endm

  /*
      ここで、EL0からEL1への例外（svc）を発生させる必要がある。
      これに対応するため、`kernel_entry` と `kernel_exit` マクロは
      例外が発生したELを示す引数 `el` を受け取る。
  */
.macro    kernel_entry, el
    sub    sp, sp, #S_FRAME_SIZE   // = 288 = 8 * 36 (x0-x30, sp, elr_el1, spsr_el1,
    stp    x0, x1, [sp, #16 * 0]   //             fp, lr)
    stp    x2, x3, [sp, #16 * 1]
    stp    x4, x5, [sp, #16 * 2]
    stp    x6, x7, [sp, #16 * 3]
    stp    x8, x9, [sp, #16 * 4]
    stp    x10, x11, [sp, #16 * 5]
    stp    x12, x13, [sp, #16 * 6]
    stp    x14, x15, [sp, #16 * 7]
    stp    x16, x17, [sp, #16 * 8]
    stp    x18, x19, [sp, #16 * 9]
    stp    x20, x21, [sp, #16 * 10]
    stp    x22, x23, [sp, #16 * 11]
    stp    x24, x25, [sp, #16 * 12]
    stp    x26, x27, [sp, #16 * 13]
    stp    x28, x29, [sp, #16 * 14]

    /* 同じタスクであっても、EL0とEL1用に2つの異なるスタックを使用している。
        これは、ユーザ空間とカーネル空間を分離したいという理由で一般的な設計である */
.if    \el == 0
    /* EL0からEL1への例外が発生すると、CPUは自動的にEL1用のSPを使用し始める。
        EL0用のSPは`sp_el0`レジスタに格納される。このレジスタの値はたとえ
        カーネルが例外ハンドラで `sp_el0` を使用しない場合でも、カーネルへの
        進入時と退出時に保存と復元を行う必要がある。各タスクごとに `sp_el0` の
        コピーを保存する必要がある。
    */
    mrs    x21, sp_el0
.else
    add    x21, sp, #S_FRAME_SIZE
.endif /* \el == 0 */

    mrs    x22, elr_el1
    mrs    x23, spsr_el1
    /* above: save reg0--29.
          below: save reg30, sp, pc(as elr_el1), pstate */
    stp    x30, x21, [sp, #16 * 15]
    stp    x22, x23, [sp, #16 * 16]

    /*
     * EL0からの例外については最終フレームレコードを作成する。
       EL1からの例外については、中断されたコードがバックトレースに
       表示されるよう、合成フレームレコードを作成する。
     */
.if \el == 0
    stp    xzr, xzr, [sp, #S_STACKFRAME]
.else
    stp    x29, x22, [sp, #S_STACKFRAME] // x29 (割り込まれた関数のfp) x22 (その関数のlr)
.endif
    add    x29, sp, #S_STACKFRAME        // x29 (fp) はフレームレコードを指す

    .endm

    .macro    kernel_exit, el
    ldp    x22, x23, [sp, #16 * 16]
    ldp    x30, x21, [sp, #16 * 15]

.if    \el == 0
    msr    sp_el0, x21
.endif /* \el == 0 */

    msr    elr_el1, x22
    /* ここでのELレベルは、例えばsyscallがカーネルに入る際に保存された
        `spsr_el1`レジスタに格納されている。したがって、以下の`eret`は
        常に例外が発生した時点のレベルに戻る。. */
    msr    spsr_el1, x23

    ldp    x0, x1, [sp, #16 * 0]
    ldp    x2, x3, [sp, #16 * 1]
    ldp    x4, x5, [sp, #16 * 2]
    ldp    x6, x7, [sp, #16 * 3]
    ldp    x8, x9, [sp, #16 * 4]
    ldp    x10, x11, [sp, #16 * 5]
    ldp    x12, x13, [sp, #16 * 6]
    ldp    x14, x15, [sp, #16 * 7]
    ldp    x16, x17, [sp, #16 * 8]
    ldp    x18, x19, [sp, #16 * 9]
    ldp    x20, x21, [sp, #16 * 10]
    ldp    x22, x23, [sp, #16 * 11]
    ldp    x24, x25, [sp, #16 * 12]
    ldp    x26, x27, [sp, #16 * 13]
    ldp    x28, x29, [sp, #16 * 14]
    add    sp, sp, #S_FRAME_SIZE
    eret
.endm


/*
 * 例外ベクトル.
 */
.align    11
.globl vectors
vectors:
    ventry    sync_invalid_el1t         // Synchronous EL1t
    ventry    irq_invalid_el1t          // IRQ EL1t
    ventry    fiq_invalid_el1t          // FIQ EL1t
    ventry    error_invalid_el1t        // Error EL1t

    ventry    sync_invalid_el1h         // Synchronous EL1h
    ventry    el1_irq                   // IRQ EL1h
    ventry    fiq_invalid_el1h          // FIQ EL1h
    ventry    error_invalid_el1h        // Error EL1h

    ventry    el0_sync                  // Synchronous 64-bit EL0 (syscall)
    ventry    el0_irq                   // IRQ 64-bit EL0
    ventry    fiq_invalid_el0_64        // FIQ 64-bit EL0
    ventry    error_invalid_el0_64      // Error 64-bit EL0

    ventry    sync_invalid_el0_32       // Synchronous 32-bit EL0
    ventry    irq_invalid_el0_32        // IRQ 32-bit EL0
    ventry    fiq_invalid_el0_32        // FIQ 32-bit EL0
    ventry    error_invalid_el0_32      // Error 32-bit EL0

sync_invalid_el1t:
    handle_invalid_entry 1,  SYNC_INVALID_EL1t

irq_invalid_el1t:
    handle_invalid_entry  1, IRQ_INVALID_EL1t

fiq_invalid_el1t:
    handle_invalid_entry  1, FIQ_INVALID_EL1t

error_invalid_el1t:
    handle_invalid_entry  1, ERROR_INVALID_EL1t

sync_invalid_el1h:
    handle_invalid_entry 1, SYNC_INVALID_EL1h

fiq_invalid_el1h:
    handle_invalid_entry  1, FIQ_INVALID_EL1h

error_invalid_el1h:
    handle_invalid_entry  1, ERROR_INVALID_EL1h

fiq_invalid_el0_64:
    handle_invalid_entry  0, FIQ_INVALID_EL0_64

error_invalid_el0_64:
    handle_invalid_entry  0, ERROR_INVALID_EL0_64

sync_invalid_el0_32:
    handle_invalid_entry  0, SYNC_INVALID_EL0_32

irq_invalid_el0_32:
    handle_invalid_entry  0, IRQ_INVALID_EL0_32

fiq_invalid_el0_32:
    handle_invalid_entry  0, FIQ_INVALID_EL0_32

error_invalid_el0_32:
    handle_invalid_entry  0, ERROR_INVALID_EL0_32

// EL1での割り込み
el1_irq:
    kernel_entry 1    // レジスタ退避
    bl  irq_handler   // 割り込み処理実行
    kernel_exit 1     // レジスタ復元

// EL0での割り込み（例外）
el0_irq:
    kernel_entry 0    // レジスタ退避
    bl  irq_handler   // 割り込み処理実行
    kernel_exit 0     // レジスタ復元

// システムコールの呼び出し (割り込みベクタより)
el0_sync:
    kernel_entry 0                      // レジスタ退避
    mrs   x25, esr_el1                  // syndromeレジスタの読み込み
    lsr   x24, x25, #ESR_ELx_EC_SHIFT   // 例外クラスを算出
    cmp   x24, #ESR_ELx_EC_SVC64        // この例外は64ビット状態で発生したか?
    b.eq  el0_svc                       // そうなら、syscallを処理
    cmp   x24, #ESR_ELx_EC_DABT_LOW     // EL0におけるデータアボートか?
    b.eq  el0_da                        // そうなら、データアボート処理
    handle_invalid_entry 0, SYNC_ERROR  // それ以外は不明なエラーとして処理

sc_nr   .req    x25                     // システムコールの数
scno    .req    x26                     // syscall番号
stbl    .req    x27                     // syscallテーブルポインタ

// システムコール処理関数（el0_syncから)
el0_svc:
    // x0-x8はカーネルにより変更されていない（依然としてEL0のまま）
    ldr   stbl, =syscalls               // syscallテーブルポインタをロード(syscall.c)
    uxtw  scno, w8                      // syscall番号はw8にセットされている
    mov   sc_nr, #__NR_syscalls
    bl    enable_irq                    // 割り込みを有効にする
    cmp   scno, sc_nr                   // syscall number >= syscall番号の上限?
    b.hs  ni_sys                        // そうなら、エラーとして処理する

    ldr   x16, [stbl, scno, lsl #3]     // syscallテーブルから処理関数のアドレスを取得
    blr   x16                           // sys_* 処理関数を呼び出す（引数はそのままx0-x7にある）
    b     ret_from_syscall              // return処理関数へジャンプ
ni_sys:
    handle_invalid_entry 0, SYSCALL_ERROR
ret_from_syscall:
    // --- kill済みのタスクの処理 ---
    // if (current->killed)
    //    exit_process(-1);
    mov   x27, sp
    and   x26, x27, #0xfffffffffffff000        // ページマスク
    add   x26, x26, #THREAD_CPU_CONTEXT
    // x26 は task_struct *current を指している
    add   x26, x26, #TASK_STRUCT_KILLED_OFFSET
    // x26 は current->killed を指している
    ldr   w27, [x26]
    cmp   w27, #0
    b.eq  cont_exit                 // killされていない場合は、exit処理を続ける
    // killされていた場合: call exit_process(-1)
    mov   x25, x0                   // syscallのエラー x0 を保存
    mov   x0, #0xffffffffffffffff   // x0 = -1
    bl    exit_process              // zombieとなり、他のタスクを再スケジュールする. cf: sched.c
    mov   x0, x25
cont_exit:
    bl    disable_irq               // 割り込みを無効にする
    str   x0, [sp, #S_X0]           // x0をユーザタスクに返す (x0 を pt_regs に保存する)
    kernel_exit 0                   // カーネルから出てユーザに戻る

el0_da:
    bl    enable_irq                // 割り込みを有効にする
    mrs   x0, far_el1
    mrs   x1, esr_el1
    mrs   x2, elr_el1
    bl    do_mem_abort              // memory abortを処理（エラーの場合とデマンドページングの場合があり）
    cmp   x0, 0                     // do_mem_abortは必ず0を返す
    b.eq  1f
    handle_invalid_entry 0, DATA_ABORT_ERROR
1:
    // --- killされたタスクを処理する (cf: 上の ret_from_syscall) ---
    mov   x27, sp
    and   x26, x27, #0xfffffffffffff000   // ページマスク
    add   x26, x26, #THREAD_CPU_CONTEXT
    // x26 は task_struct *current を指している
    add   x26, x26, #TASK_STRUCT_KILLED_OFFSET
    // x26 は current->killed を指している
    ldr   w27, [x26]
    cmp   w27, #0
    b.eq  2f                              // killされていないので、終了処理へ
    // killされていた: exit_process(-1) を呼び出す
    mov   x25, x0                         // x0 を保存
    mov   x0, #0xffffffffffffffff         // x0 = -1
    bl    exit_process                    // zombieとなり、他のタスクを再スケジュールする. cf: sched.c
    mov   x0, x25
2:
    bl    disable_irq                     // 割り込みを無効にする
    kernel_exit 0

// 新規タスクにより実行される最初の命令
// x19 = fn; x20 = arg, copy_process() で設定されている
.globl ret_from_fork
ret_from_fork:
    bl    leave_scheduler       // cf. kernel/sched.c
    cbz   x19, ret_to_user      // 新規タスクがユーザタスクの場合
    mov   x0, x20               // 新規タスクがカーネルタスクの場合
    blr   x19                   // fn(arg)を実行
    // カーネルスレッドは復帰すると処理を続行してユーザ空間に戻る
ret_to_user:
    bl    disable_irq           // 割り込みを無効にする
    kernel_exit 0               // pt_regsを復元し、eretを実行してEL0に移行する

// ハングアップ
.globl err_hang
err_hang:
    b err_hang
