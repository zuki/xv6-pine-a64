#include <types.h>
#include <timer.h>
#include <arm.h>
#include <irq.h>
#include <console.h>

// Timer 1をタイマーとして使用する
// FIXME: 将来、タイマーとして使用する場合、この関数は不要になる
void timer_init(void)
{
    // 当面、1秒タイマーとして使用する
    put32(TMR1_INTV_VALUE_REG, 120000);
    timer_reset();
    put32(TMR1_CTRL_REG, TMR_MODE_SINGLE | TMR_CLK_PRES(1) | TMR_CLK_SRC_24M);
    put32(TMR1_CTRL_REG, get32(TMR1_CTRL_REG) | TMR_RELOAD);
    while(get32(TMR1_CTRL_REG) & TMR_RELOAD);
    put32(TMR1_CTRL_REG, get32(TMR1_CTRL_REG) | TMR_EN);

    irq_enable(IRQ_TIMER1);
    irq_register(IRQ_TIMER1, timer_intr, 0);

    info("timer_init ok");
}

/*
 * 現在はタイマーをリロードするだけで特に何もしていない。
 */
void timer_intr(void *param)
{
    // reload
    timer_reset();
    // 時刻更新処理
    //update_proc_time();
}

void timer_reset(void)
{
    put32(TMR1_CTRL_REG, get32(TMR1_CTRL_REG) | TMR_RELOAD);
}
