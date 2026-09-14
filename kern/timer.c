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
    // 1. Timer1を有効にする
    put32(TMR_IRQ_EN_REG, get32(TMR_IRQ_EN_REG) | 0x2);
    // 2. シングルモード、24MHzクロック、2プリスケールを選択
    put32(TMR1_CTRL_REG, TMR_MODE_SINGLE | TMR_CLK_PRES(1) | TMR_CLK_SRC_24M);
    // 3. インターバル値をセット
    put32(TMR1_INTV_VALUE_REG, 120000);
    // 4. リロードビットをセット
    put32(TMR1_CTRL_REG, get32(TMR0_CTRL_REG) | TMR_RELOAD);
    // 5. リロードが終わるのを待つ
    while(get32(TMR1_CTRL_REG) & TMR_RELOAD);
    // 6. タイマー1を開始する
    put32(TMR1_CTRL_REG, get32(TMR1_CTRL_REG) | TMR_EN);

    irq_register(IRQ_TIMER0, timer_intr, 0);
    irq_enable(IRQ_TIMER0);

    info("timer_init ok");
}

/*
 * 現在はタイマーをリロードするだけで特に何もしていない。
 */
void timer_intr(void *param)
{
    // pendingビットをクリア
    put32(TMR_IRQ_STA_REG, 0x2);
    cprintf("+");
}

void timer_reset(void)
{
    put32(TMR1_CTRL_REG, get32(TMR1_CTRL_REG) | TMR_RELOAD);
    while(get32(TMR1_CTRL_REG) & TMR_RELOAD);
}

