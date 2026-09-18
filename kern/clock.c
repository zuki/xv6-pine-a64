#include <clock.h>

#include <arm.h>
#include <irq.h>
#include <arm_timer.h>
#include <console.h>

// Timer 0をクロックタイマーとして使用する
static uint64_t jiffies = 0;

void clock_init(void)
{
    arm_vtimer_init();
    irq_register(GIC_PPI(11), clock_intr, 0);
    irq_enable(GIC_PPI(11));

    info("clock_init ok");
}

void clock_intr(void *param)
{
    // pendingビットをクリア
    arm_vtimer_reset();
    if ((++jiffies % 100) == 0)
        cprintf("*");
    // 時刻更新処理
    //update_proc_time();
}


#if 0
void clock_init(void)
{
    // Timer0は10ms毎に割り込んでtickを進める
    // Timer0/1を有効にする
    put32(TMR_IRQ_EN_REG, get32(TMR_IRQ_EN_REG) | 0x1);
    debug("TMR_IRQ_EN_REG: 0x%x", get32(TMR_IRQ_EN_REG));
    debug("IRQ_TIMER0: %d", IRQ_TIMER0);
    // 2. 自動リロード、24MHzクロック、2プリスケールを選択
    put32(TMR0_CTRL_REG, TMR_MODE_CONT | TMR_CLK_PRES(1) | TMR_CLK_SRC_24M);
    // 3. インターバル値をセット
    put32(TMR0_INTV_VALUE_REG, 1200);
    // 4. リロードビットをセット
    put32(TMR0_CTRL_REG, get32(TMR0_CTRL_REG) | TMR_RELOAD);
    // 5. リロードが終わるのを待つ
    while(get32(TMR0_CTRL_REG) & TMR_RELOAD);
    // 6. タイマー0を開始する
    put32(TMR0_CTRL_REG, get32(TMR0_CTRL_REG) | TMR_EN);

    irq_register(IRQ_TIMER0, clock_intr, 0);
    irq_enable(IRQ_TIMER0);


    // Timer0は10ms毎に割り込んでtickを進める
    // 1, HSTimerを有効にする
    put32(HS_TMR_IRQ_EN_REG, 0x1);
    // 2. n_mode, 2 pre-scale, 自動リロードを選択
    put32(HS_TMR_CTRL_REG, HS_TMR_CTRL_NORMAL | HS_TMR_CTRL_MODE_CONT | HS_TMR_CTRL_CLK(1) );
    // 3. インターバル値をセット
    put32(HS_TMR_INTV_HI_REG, 0);
    put32(HS_TMR_INTV_LO_REG, 500000);
    // 4. リロードビットをセット
    put32(HS_TMR_CTRL_REG, get32(HS_TMR_CTRL_REG) | HS_TMR_CTRL_RELOAD);
    // 6. タイマー0を開始する
    put32(HS_TMR_CTRL_REG, get32(HS_TMR_CTRL_REG) | HS_TMR_CTRL_EN);

    irq_register(IRQ_HSTIMER, clock_intr, 0);
    irq_enable(IRQ_HSTIMER);

void clock_reset(void)
{
    put32(TMR0_CTRL_REG, get32(TMR0_CTRL_REG) | TMR_RELOAD);
    while(get32(TMR0_CTRL_REG) & TMR_RELOAD);
}

/*
 * TODO: tickを進める
 */
void clock_intr(void *param)
{
    // pendingビットをクリア
    //put32(TMR_IRQ_STA_REG, 0x1);
    put32(HS_TMR_STAS_REG, 0x1);
    if ((++jiffies2 % 100) == 0)
        cprintf("#");
    // 時刻更新処理
    //update_proc_time();
}
#endif



