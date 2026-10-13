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


// 現在の仮想カウント値
// uint64_t timestamp(void)
.globl timestamp
timestamp:
  mrs   x0, cntpct_el0
  ret

// レジスタxに値pを書き込む
// void put32(uint64 *addr, uint32_t x)
.globl put32
put32:
  str   w1, [x0]
  ret

// レジスタpの値を読み取る
// uint32_t get32(uint64_t *addr);
.globl get32
get32:
  ldr   w0, [x0];
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

