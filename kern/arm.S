#include <types.h>

/* Instruction synchronization barrier. */
// void isb(void);
.globl isb;
isb:
  isb
  ret

/* Data synchronization barrier. */
// void dsb(void);
.globl dsb
dsb:
  dsb   sy
  ret

/* Data memory barrier. */
// void dmb(void);
.globl dmb
dmb:
  dmb   sy
  ret

/* Brute-force data and instruction synchronization barrier. */
// oid disb(void);
.globl disb
disb:
  dsb   sy
  isb
  ret

// void delay(uint64_t cycles)
.globl delay
delay:
  subs  x0, x0, #1
  bne   delay
  ret

// uint64_t timerfreq(void)
.globl timerfreq
timerfreq:
  mrs x0, cntfrq_el0
  ret

// 現在の仮想カウント値
// uint64_t timestamp(void)
.globl timestamp
timestamp:
  mrs   x0, cntpct_el0
  ret

// レジスタxに値pを書き込む
// void put32(uint32_t *addr, uint32_t x)
.globl put32
put32:
  mov   x2, x0
  str   w1, [x2]
  ret

// レジスタpの値を読み取る
// uint32_t get32(uint32_t *addr);
.globl get32
get32:
  mov   x1, x0
  ldr   w0, [x1];
  ret

// アドレスpをベクタ基底アドレスレジスタに書き込む
// void load_vbar_el1(uint64_t *addr);
.globl load_vbar_el1
load_vbar_el1:
  msr   vbar_el1, x0
  ret

// void load_sp_el1(uint64_t *addr);
.globl load_sp_el1
load_sp_el1:
  msr   sp_el1, x0
  ret

// cpuidを取得する: int cpuid(void);
.globl cpuid
cpuid:
  mrs   x1, mpidr_el1
  and   w0, w1, #255
  ret

/* Read Exception Syndrome Register (EL1) */
// uint64_t resr(void);
.globl resr
resr:
  mrs   x0, esr_el1
  ret

/* Load Exception Syndrome Register (EL1) */
// void lesr(uint64_t v);
.globl lesr
lesr:
  msr   esr_el1, x0
  ret


/* Read Exception Link Register (EL1) */
// uint64_t relr(void);
.globl relr
relr:
  mrs   x0, elr_el1
  ret

/* Read Fault Address Register (EL1) */
// uint64_t rfar(void);
.globl rfar
rfar:
  mrs   x0, far_el1
  ret

/* 割込み(darfすべて）を有効にする: void sti(void) */
.globl sti
sti:
  msr   daif, xzr
  ret

/* 割込み(darfすべて）を無効にする: void cli(void) */
.globl cli
cli:
  mov   x0, #15
  lsr   x1, x0, 6
  msr   daif, x1
  ret

// 割り込みを有効にする: void enable_irq(void);
.globl enable_irq
enable_irq:
  msr   daifclr, #2
  ret

// void wfi(void);
.globl wfi
wfi:
  wfi
  ret

// void wfe(void);
.globl wfe
wfe:
  wfe
  ret

// void sev(void);
.globl sev
sev:
  sev
  ret

// 割り込みを無効にする: void diable_irq(void);
.globl disable_irq
disable_irq:
  msr   daifset, #2
  ret

// 高速割り込みを有効にする: void enable_fiq(void);
.globl enable_fiq
enable_fiq:
  msr   daifclr, #1
  ret

// 高速割り込みを無効にする: void diable_fiq(void);
.globl disable_fiq
disable_fiq:
  msr   daifset, #1
  ret

// 割り込みがマスクされているか: int is_irq_masked(void);
.globl is_irq_masked
is_irq_masked:
  mrs   x0, daif
  lsr   x0, x0, #7
  and   x0, x0, #1
  ret

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

// int get_el(void);
.globl get_el
get_el:
    mrs x0, CurrentEL
    lsr x0, x0, #2
    ret
