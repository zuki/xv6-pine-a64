# 0 "kern/entry.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "kern/entry.S"
# 1 "inc/mmu.h" 1
# 2 "kern/entry.S" 2
# 1 "inc/memlayout.h" 1
# 3 "kern/entry.S" 2
# 1 "inc/param.h" 1
# 4 "kern/entry.S" 2
# 1 "inc/sysregs.h" 1
# 5 "kern/entry.S" 2
# 1 "inc/sched.h" 1



# 1 "inc/types.h" 1
# 5 "inc/sched.h" 2
# 1 "inc/linux/time.h" 1




# 1 "inc/list.h" 1
# 6 "inc/linux/time.h" 2







typedef void(*fn)(uint64_t);

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};

struct timer_list {
    struct list_head list;
    uint64_t expires;
    uint64_t data;
    void (*fn)(uint64_t);
};

extern uint64_t jiffies;
extern struct timespec xtime;
# 63 "inc/linux/time.h"
extern void add_timer(struct timer_list *timer);

extern int del_timer(struct timer_list *timer, boolean del);





int mod_timer(struct timer_list *timer, uint64_t expires);

static inline void init_timer(struct timer_list * timer)
{
 timer->list.next = timer->list.prev = ((void *)0);
}

static inline int timer_pending(const struct timer_list * timer)
{
 return timer->list.next != ((void *)0);
}







void init_timervecs(void);
void run_timer_list(void);
long getitimer(int, struct itimerval *);
void it_real_fn(uint64_t);
long setitimer(int, struct itimerval *, struct itimerval *);




static inline void timerclear(struct timeval *tv) {
    tv->tv_sec = 0;
    tv->tv_usec = 0;
}

static inline void timersub(struct timeval *a, struct timeval *b, struct timeval *res) {
    res->tv_sec = a->tv_sec - b->tv_sec;
    res->tv_usec = a->tv_usec - b->tv_usec;
    if (res->tv_usec < 0) {
        --res->tv_sec;
        res->tv_usec += 1000*1000;
    }
}
# 6 "inc/sched.h" 2
# 6 "kern/entry.S" 2
# 1 "inc/peripherals/uart.h" 1



# 1 "inc/peripherals/base.h" 1
# 5 "inc/peripherals/uart.h" 2
# 7 "kern/entry.S" 2

.section ".text.boot"

cpuid_reg .req x28




.globl _start
.globl mp_start

_start:

    bl __asm_flush_dcache_all

    mrs cpuid_reg, mpidr_el1
    and cpuid_reg, cpuid_reg, #3


    mrs x1, CurrentEL
    and x1, x1, #0xc
    cmp x1, #4
    b.eq el1
    cmp x1, #8
    b.eq el2
    b.gt el3

el3:
    mov x2, #((3 << 4) | (1 << 10) | (1 << 0))
    msr scr_el3, x2
    mov x2, #((7 << 6) | (9 << 0))
    msr spsr_el3, x2
    adr x2, el2
    msr elr_el3, x2
    eret

el2:

    mrs x0, cnthctl_el2
    orr x0, x0, #3
    msr cnthctl_el2, x0
    msr cntvoff_el2, xzr


    mov x0, #(1 << 31)
    msr hcr_el2, x0


    ldr x2, =((3 << 28) | (3 << 22) | (1 << 20) | (1 << 11) | (0 << 25) | (0 << 12) | (0 << 2) | (0 << 0))
    msr sctlr_el1, x2
    dsb sy
    isb


    mov x2, #((7 << 6) | (5 << 0))
    msr spsr_el2, x2

    adr x2, el1
    msr elr_el2, x2
    eret

el1:
    cbnz cpuid_reg, setup_sp




    ldr x0, =bss_begin
    ldr x1, =bss_end
    ldr x2, =0x0
    sub x0, x0, x2
    sub x1, x1, x2
    sub x1, x1, x0
    bl memzero_aligned

setup_sp:


    mov x0, #4096
    ldr x1, =boot_stacks
    add x2, cpuid_reg, #1
    mul x0, x0, x2
    add x0, x0, x1
    msr spsel, #1
    mov sp, x0
# 103 "kern/entry.S"
    cbnz cpuid_reg, secondary_core_setup


    adr x0, kpgdir
    msr ttbr0_el1, x0
    msr ttbr1_el1, x0
    isb

    ldr x0, =((64 - 48) | ((64 - 48) << 16) | (0 << 14) | (2 << 30) | (3 << 12) | (3 << 28) | ((1 << 10) | (1 << 8)) | ((1 << 26) | (1 << 24)))
    msr tcr_el1, x0
    isb

    ldr x0, =((0x00 << (8 * 0x0)) | (0xFF << (8 * 0x1)) | (0x44 << (8 * 0x2)))
    msr mair_el1, x0


    ldr x0, =vectors
    msr vbar_el1, x0

    ldr x2, =main
    ldr x0, =( (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11) | (1 << 12) | (1 << 3) | (1 << 4) | (1 << 2) | (1 << 0) )
    msr sctlr_el1, x0
    br x2
# 137 "kern/entry.S"
secondary_core_asm:
    mov x0, cpuid_reg
    bl secondary_main


secondary_core_setup:

    adr x1, kpgdir
    msr ttbr0_el1, x1
    msr ttbr1_el1, x1
    isb

    ldr x1, =((64 - 48) | ((64 - 48) << 16) | (0 << 14) | (2 << 30) | (3 << 12) | (3 << 28) | ((1 << 10) | (1 << 8)) | ((1 << 26) | (1 << 24)))
    msr tcr_el1, x1
    ldr x1, =((0x00 << (8 * 0x0)) | (0xFF << (8 * 0x1)) | (0x44 << (8 * 0x2)))
    msr mair_el1, x1
    isb

    ldr x1, =vectors
    msr vbar_el1, x1
    isb

    ldr x2, =secondary_core_asm


    ldr x1, =( (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11) | (1 << 12) | (1 << 3) | (1 << 4) | (1 << 2) | (1 << 0) )
    msr sctlr_el1, x1
    isb

    br x2
