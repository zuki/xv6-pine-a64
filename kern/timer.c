#include <types.h>
#include <timer.h>
#include <arm_timer.h>
#include <arm.h>
#include <irq.h>
#include <console.h>
#include <param.h>

//static volatile uint64_t core_jiffies2[NCPU] = { 0 };
//static int core_ids[NCPU] = {0, 1, 2, 3};

// 物理タイマーをタイマーとして使用する: すべてのコアで実行
void timer_init(int core)
{
    arm_ptimer_init(240000);
    irq_register(GIC_PPI(14), timer_intr, NULL);
    irq_enable(GIC_PPI(14));
    info("timer_init [%d]: arg: %p", core, &core);
}

void timer_intr(void *param)
{

    // pendingビットをクリア
    arm_ptimer_reset(240000);
#if 0
    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    int core = mpidr & 0xFF;

    core_jiffies2[core]++;
    if ((core_jiffies2[core] % 100) == 0)
        cprintf("%d", core);
#endif
}

#define TIMER_CLOCK         (24 * 1000 * 1000)
#define COUNT_TO_USEC(x)    ((x) / 24)
#define USEC_TO_COUNT(x)    ((x) * 24)
#define TICKS_PER_HZ        (TIMER_CLOCK / 1000)   // CONFIG_SYS_HZ
#define TICKS_TO_HZ(x)      ((x) / TICKS_PER_HZ)
#define TIMER_LOAD_VAL      ((uint64_t)-1)

/* read the 32-bit timer */
static uint64_t read_timer(void)
{
    return arm_system_counter();
}

static uint64_t get_timer_masked(void)
{
    static uint64_t tbl = 0;
    static uint64_t lastinc = 0;

    /* current tick value */
    uint64_t now = TICKS_TO_HZ(read_timer());

    if (now >= lastinc) {   /* normal (non rollover) */
        tbl += (now - lastinc);
    } else {                /* rollover */
        tbl += (TICKS_TO_HZ(TIMER_LOAD_VAL) - lastinc) + now;
    }
    lastinc = now;

    return tbl;
}

/* 割り込みなしのタイマー */
uint64_t get_timer(uint64_t base)
{
    return get_timer_masked() - base;
}
