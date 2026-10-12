// ----------------------- pgd --------------------------------------------//
// ttbr0, ユーザ仮想アドレス

// pgdをセットする: void set_pgd(unsigned long pgd);
.globl set_pgd
set_pgd:
    msr   ttbr0_el1, x0
    tlbi  vmalle1is     // Inner Shareableの全仮想アドレスのTLBを無効化
    dsb   ish           // TLB無効化が完了することを保証
    isb                 // パイプラインをクリア
    ret

// pgdを取得する: unsigned long get_pgd(void);
.globl get_pgd
get_pgd:
    mov   x1, 0
    ldr   x0, [x1]
    mov   x0, 0x1000
    msr   ttbr0_el1, x0
    ldr   x0, [x1]
    ret

// ttbr1 カーネル仮想アドレス

// kpgdirをセットする: void set_kpgdir(void);
.globl  set_kpgdir
set_kpgdir:
    adr   x1, kpgdir
    msr   ttbr1_el1, x1
    tlbi  vmalle1is
    dsb   ish
    isb
    ret

// ---------------------- misc -------------------------------------------- //
// _aligned 関数は通常のバージョンよりも高速ですが、厄介なバグを避けるため、
// 細心の注意を払って使用する必要がある（アラインメントが不適切なアドレスを使用すると、
// 内容が破損したり読み取り漏れが発生したりする）。
// バッファが十分に大きくない限り、その速度向上に見合う価値はない。

// void* memcpy_aligned(void* dst, const void* src, unsigned int n);
.globl memcpy_aligned
memcpy_aligned:
    ldr x3, [x1], #8
    str x3, [x0], #8
    subs x2, x2, #8
    b.gt memcpy_aligned
    ret

// void memzero_aligned(void *src, unsigned long n);
.globl memzero_aligned
memzero_aligned:
    str xzr, [x0], #8
    subs x1, x1, #8
    b.gt memzero_aligned
    ret

// int get_el(void);
.globl get_el
get_el:
    mrs x0, CurrentEL
    lsr x0, x0, #2
    ret

// aarch64 キャッシュ操作関数, from Rt thread OS
// https://raw.githubusercontent.com/RT-Thread/rt-thread/master/libcpu/aarch64/common/cache.S

/*
 * void __asm_flush_dcache_line(void *addr);
 */
.globl __asm_flush_dcache_line
__asm_flush_dcache_line:
    dc    cvac, x0
    dsb   sy
    ret

/*
 * void __asm_invalidate_dcache_line(void *addr);
 */
.globl __asm_invalidate_dcache_line
__asm_invalidate_dcache_line:
    dc    ivac, x0
    dsb   sy
    ret

/*
 * void __asm_flush_dcache_range(start, end)
 *
 * 指定範囲のデータキャッシュをクリアして無効化する
 *
 * x0: 開始アドレス
 * x1: 終了アドレス
  * (fxl: 両方とも仮想アドレス, "dc civac" - dc cache and/or inv by VA)
 */
.globl __asm_flush_dcache_range
__asm_flush_dcache_range:
    mrs    x3, ctr_el0
    lsr    x3, x3, #16
    and    x3, x3, #0xf
    mov    x2, #4
    lsl    x2, x2, x3        /* キャッシュラインサイズ */

    /* x2 <- キャッシュシステムの最小キャッシュラインサイズ */
    sub    x3, x2, #1
    bic    x0, x0, x3

1:  dc    civac, x0    /* データキャッシュまたは統合キャッシュをクリアして無効化 */
    add    x0, x0, x2
    cmp    x0, x1
    b.lo    1b
    dsb    sy
    ret

/* void __asm_invalidate_dcache_range(start, end)
 *
 * 指定範囲のデータキャッシュを無効化する
 *
 * x0: start address
 * x1: end address
 */
.globl __asm_invalidate_dcache_range
__asm_invalidate_dcache_range:
    mrs    x3, ctr_el0
    lsr    x3, x3, #16
    and    x3, x3, #0xf
    mov    x2, #4
    lsl    x2, x2, x3        /* cache line size */

    /* x2 <- minimal cache line size in cache system */
    sub    x3, x2, #1
    bic    x0, x0, x3

1:  dc   ivac, x0    /* invalidate data or unified cache */
    add    x0, x0, x2
    cmp    x0, x1
    b.lo    1b
    dsb    sy
    ret

// from Rt thread OS
// https://raw.githubusercontent.com/RT-Thread/rt-thread/master/libcpu/aarch64/common/cache.S
/*
 * void __asm_dcache_level(level)
 *
 * 指定のレベルのキャッシュをフラッシュまたは無効化する
 *
 * x0: cache level
 * x1: 0 clean & invalidate, 1 invalidate only
 * x2~x9: clobbered
 */
.globl __asm_dcache_level
__asm_dcache_level:
    lsl    x12, x0, #1
    msr    csselr_el1, x12        /* select cache level */
    isb                /* sync change of cssidr_el1 */
    mrs    x6, ccsidr_el1        /* read the new cssidr_el1 */
    and    x2, x6, #7        /* x2 <- log2(cache line size)-4 */
    add    x2, x2, #4        /* x2 <- log2(cache line size) */
    mov    x3, #0x3ff
    and    x3, x3, x6, lsr #3    /* x3 <- max number of #ways */
    clz    w5, w3            /* bit position of #ways */
    mov    x4, #0x7fff
    and    x4, x4, x6, lsr #13    /* x4 <- max number of #sets */
    /* x12 <- cache level << 1 */
    /* x2 <- line length offset */
    /* x3 <- number of cache ways - 1 */
    /* x4 <- number of cache sets - 1 */
    /* x5 <- bit position of #ways */

loop_set:
    mov    x6, x3            /* x6 <- working copy of #ways */
loop_way:
    lsl    x7, x6, x5
    orr    x9, x12, x7        /* map way and level to cisw value */
    lsl    x7, x4, x2
    orr    x9, x9, x7        /* map set number to cisw value */
    tbz    w1, #0, 1f
    dc    isw, x9
    b    2f
1:    dc    cisw, x9        /* clean & invalidate by set/way */
2:    subs    x6, x6, #1        /* decrement the way */
    b.ge    loop_way
    subs    x4, x4, #1        /* decrement the set */
    b.ge    loop_set

    ret

/*
 * void __asm_flush_dcache_all(int invalidate_only)
 *
 * x0: 0 clean & invalidate, 1 invalidate only
 *
 * flush or invalidate all data cache by SET/WAY.
 */
.globl __asm_dcache_all
__asm_dcache_all:
    mov    x1, x0
    dsb    sy
    mrs    x10, clidr_el1        /* read clidr_el1 */
    lsr    x11, x10, #24
    and    x11, x11, #0x7        /* x11 <- loc */
    cbz    x11, finished        /* if loc is 0, exit */
    mov    x15, lr
    mov    x0, #0            /* start flush at cache level 0 */
    /* x0  <- cache level */
    /* x10 <- clidr_el1 */
    /* x11 <- loc */
    /* x15 <- return address */

loop_level:
    lsl    x12, x0, #1
    add    x12, x12, x0        /* x0 <- tripled cache level */
    lsr    x12, x10, x12
    and    x12, x12, #7        /* x12 <- cache type */
    cmp    x12, #2
    b.lt    skip            /* skip if no cache or icache */
    bl    __asm_dcache_level    /* x1 = 0 flush, 1 invalidate */
skip:
    add    x0, x0, #1        /* increment cache level */
    cmp    x11, x0
    b.gt    loop_level

    mov    x0, #0
    msr    csselr_el1, x0        /* restore csselr_el1 */
    dsb    sy
    isb
    mov    lr, x15

finished:
    ret

.globl __asm_flush_dcache_all
__asm_flush_dcache_all:
    mov    x0, #0
    b    __asm_dcache_all

.globl __asm_invalidate_dcache_all
__asm_invalidate_dcache_all:
    mov    x0, #0x1
    b    __asm_dcache_all


