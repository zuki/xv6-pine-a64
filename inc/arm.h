#ifndef INC_ARM_H
#define INC_ARM_H

#ifndef __ASSEMBLER__

#include <types.h>
#include <param.h>

void isb(void);
void dsb(void);
void dmb(void);
void disb(void);
void delay(uint64_t cycles);
uint64_t timestamp(void);
void put32(uint64_t *addr, uint32_t x);
uint32_t get32(uint64_t *addr);
void load_vbar_el1(uint64_t *addr);
void load_sp_el1(uint64_t *addr);
int cpuid(void);
uint64_t resr(void);
void lesr(uint64_t v);
uint64_t relr(void);
uint64_t rfar(void);
void sti(void);
void cli(void);
void enable_irq(void);
void disable_irq(void);
void enable_fiq(void);
void disable_fiq(void);
int  is_irq_masked(void);

static inline int intr_get(void) {
    return 1 - is_irq_masked();
}

#define get32va(x)      get32(PA2VA(x))
#define put32va(x,v)    put32(PA2VA(x), v)

#endif      // __ASSEMBLER__

#endif
