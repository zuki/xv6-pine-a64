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

static inline void put32(uint64_t p, uint32_t x)
{
    *(volatile uint32_t *)p = x;
}

static inline uint32_t get32(uint64_t p)
{
    return *(volatile uint32_t *)p;
}

/*
 * Load vector base address register
 */
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
static inline void
lesr(uint64_t _v)
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

#endif
