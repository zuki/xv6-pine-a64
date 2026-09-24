#ifndef INC_ARM_H
#define INC_ARM_H

#include <types.h>

/* Prevent compiler from reordering. */
static inline void
barrier()
{
    __asm__ __volatile__("" ::: "memory");
}

/* Instruction synchronization barrier. */
static inline void
isb()
{
    __asm__ __volatile__("isb" ::: "memory");
}

/* Data synchronization barrier. */
static inline void
dsb()
{
    __asm__ __volatile__("dsb sy" ::: "memory");
}

/* Data memory barrier. */
static inline void
dmb()
{
    __asm__ __volatile__("dmb sy" ::: "memory");
}

/* Brute-force data and instruction synchronization barrier. */
static inline void
disb()
{
    dsb();
    isb();
}

static inline void delay(int32_t count)
{
    asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n":
                 "=r"(count): [count]"0"(count) : "cc");
}

// 現在の仮想カウント値
static inline uint64_t timestamp()
{
    uint64_t t;
    barrier();
    __asm__ __volatile__ ("mrs %[cnt], cntpct_el0" : [cnt]"=r"(t));
    barrier();
    return t;
}

/* cntfrq_el0 = 24000000 : 24MHz */
static inline uint64_t timerfreq()
{
    uint64_t f;
    __asm__ __volatile__ ("mrs %[freq], cntfrq_el0" : [freq]"=r"(f));
    return f;
}

/* マイクロ秒待機. */
static inline void delayus(uint32_t n)
{
    uint64_t f = timerfreq(), t = timestamp(), r;
    /* Calculate expire value for counter. */
    t += f / 1000000 * n;
    do {
        r = timestamp();
    } while (r < t);
}

// レジスタxに値pを書き込む
static inline void put32(uint64_t p, uint32_t x)
{
    *(volatile uint32_t *)p = x;
}

// レジスタpの値を読み取る
static inline uint32_t get32(uint64_t p)
{
    return *(volatile uint32_t *)p;
}

// アドレスpをベクタ基底アドレスレジスタに書き込む
static inline void load_vbar_el1(void *p)
{
    disb();
    asm volatile("msr vbar_el1, %[x]": : [x]"r"(p));
    disb();
}

static inline void load_sp_el1(void *p)
{
    disb();
    asm volatile("msr sp_el1, %[x]": : [x]"r"(p));
    disb();
}

// cpuidを取得する: int cpuid(void);
static inline int cpuid()
{
    int64_t id;
    __asm__ __volatile__("mrs %[x], mpidr_el1" : [x]"=r"(id));
    return id & 0xFF;
}

/* Read Exception Syndrome Register (EL1) */
static inline uint64_t resr()
{
    disb();
    uint64_t r;
    __asm__ __volatile__("mrs %[x], esr_el1" : [x]"=r"(r));
    disb();
    return r;
}

/* Load Exception Syndrome Register (EL1) */
static inline void lesr(uint64_t _v)
{
    disb();
    __asm__ __volatile__("msr esr_el1, %[x]" : : [x]"r"(0));
    disb();
}

/* Read Exception Link Register (EL1) */
static inline uint64_t relr()
{
    disb();
    uint64_t r;
    __asm__ __volatile__("mrs %[x], elr_el1" : [x]"=r"(r));
    disb();
    return r;
}

/* Read Fault Address Register (EL1) */
static inline uint64_t rfar()
{
    disb();
    uint64_t r;
    __asm__ __volatile__("mrs %[x], far_el1" : [x]"=r"(r));
    disb();
    return r;
}

/* 割込み(darfすべて）を有効にする: void sti(void) */
static inline void sti(void)
{
    asm volatile("msr daif, %[x]" : : [x]"r"(0));
}

/* 割込み(darfすべて）を無効にする: void cli(void) */
static inline void cli(void)
{
    asm volatile("msr daif, %[x]" : : [x]"r"(0xF << 6));
}

// 割り込みを有効にする: void enable_irq(void);
static inline void enable_irq(void)
{
    __asm__ __volatile__ ("msr DAIFClr, #2");
}

// 割り込みを無効にする: void diable_irq(void);
static inline void disable_irq(void)
{
    __asm__ __volatile__ ("msr DAIFSet, #2");
}

// 高速割り込みを有効にする: void enable_fiq(void);
static inline void enable_fiq(void)
{
    __asm__ __volatile__ ("msr DAIFClr, #1");
}

// 高速割り込みを無効にする: void diable_fiq(void);
static inline void disable_fiq(void)
{
    __asm__ __volatile__ ("msr DAIFSet, #1");
}

// 割り込みがマスクされているか: int irq_enabled(void);
static inline int irq_enabled(void)
{
    uint64_t r;
    __asm__ __volatile__("mrs %[x], daif" : [x]"=r"(r));
    return (r & 0x80) == 0;
}

#endif
