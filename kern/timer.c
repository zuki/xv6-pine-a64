#include <types.h>
#include <timer.h>
#include <arm.h>
#include <irq.h>
#include <console.h>

static uint64_t jiffies2 = 0;

// 物理タイマーをタイマーとして使用する: すべてのコアで実行
void timer_init(void)
{
    arm_ptimer_init();
    irq_register(GIC_PPI(14), timer_intr, 0);
    irq_enable(GIC_PPI(14));
    info("timer_init ok");
}

void timer_intr(void *param)
{
    // pendingビットをクリア
    arm_ptimer_reset();
    if ((++jiffies2 % 200) == 0)
        cprintf("$");
}



