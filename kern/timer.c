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
