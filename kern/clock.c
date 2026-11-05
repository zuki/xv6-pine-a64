#include <types.h>
#include <clock.h>
#include <irq.h>
#include <rtc.h>
#include <console.h>
#include <vm.h>
#include <linux/errno.h>
#include <linux/time.h>

// Timer 0をクロックタイマーとして使用する
uint64_t jiffies = 0;
/* 現在時刻 (wall_time) */
struct timespec xtime  __attribute__ ((aligned (16)));
/* 直近のwall_time更新時のjiffies */
uint64_t wall_jiffies = 0;
/* jiffies, xtime, wall_jiffies を保護 */
struct spinlock clocklock;

uint64_t tick_usec = TICK_USEC;        /* USER_HZ period (usec) */
uint64_t tick_nsec = TICK_NSEC;        /* ACTHZ period (nsec) */

// 現在時の更新
static void update_wall_time(uint64_t ticks)
{
    do {
        ticks--;
        xtime.tv_nsec += TICK_NSEC;         // 1 tick = 10ms = 10 * 10^6
        if (xtime.tv_nsec >= 1000000000) {  // nsec部分が1秒を超えたら
            xtime.tv_nsec -= 1000000000;    // nsecから1秒引いて
            xtime.tv_sec++;                 // secに1秒足す
        }
    } while (ticks);
}

// 現在時の調整
static inline void update_times(void)
{
    uint64_t ticks;

    ticks = jiffies - wall_jiffies;
    if (ticks) {
        wall_jiffies += ticks;
        update_wall_time(ticks);
    }
}

void clock_init(void)
{
    arm_vtimer_init();
    irq_register(GIC_PPI(11), clock_intr, 0);
    irq_enable(GIC_PPI(11));

    initlock(&clocklock, "clock");

    // rtc_init()で設定
#if 0
    if (rtc_gettime(&xtime) < 0) {
        xtime.tv_nsec = 0L;
        xtime.tv_sec = 1793844559;  // 2026-11-05 11:09:19
    }
#endif

    info("clock_init ok");
}

void clock_intr(void *param)
{
    acquire(&clocklock);
    ++jiffies;
    update_times();
    //wakeup(&jiffies);
    release(&clocklock);

    //run_timer_list();

    arm_vtimer_reset();
}

long clock_gettime(clockid_t clk_id, uint64_t tpp)
{
    struct timespec tp;
    struct proc *p = thisproc();
    int ret = 0;

    switch(clk_id) {
        default:
            ret = -EINVAL;
        case CLOCK_REALTIME:
            acquire(&clocklock);
            tp.tv_nsec = xtime.tv_nsec;
            tp.tv_sec = xtime.tv_sec;
            release(&clocklock);
            ret = copyout(p->pgdir, (struct timespec *)tpp, &tp, sizeof(tp));
            break;
        case CLOCK_PROCESS_CPUTIME_ID:
#if 0
            acquire(&clocklock);
            ptime = (p->stime + p->utime) * TICK_NSEC;
            tp->tv_nsec = ptime % 1000000000;
            tp->tv_sec  = ptime / 1000000000;
            release(&clocklock);
#endif
            ret = -EINVAL;
            break;
    }
#if 0
    static int count = 0;
    if ((++count % 1000) == 0)
        debug("[%d] clk: %d, tv_sec: %lld, tv_nsec: %lld", count, clk_id, tp->tv
_sec, tp->tv_nsec);
#endif
    return ret;
}

long clock_settime(clockid_t clk_id, uint64_t tpp)
{
    struct timespec tp;
    int ret = 0;

    switch(clk_id) {
        default:
            ret = -EINVAL;
        case CLOCK_REALTIME:
            //if (capable(CAP_SYS_TIME)) {
            if ((ret = copyin(thisproc()->pgdir, (char *)&tp, tpp, sizeof(tp))) < 0)
                break;
            acquire(&clocklock);
            xtime.tv_nsec = tp.tv_nsec;
            xtime.tv_sec = tp.tv_sec;
            release(&clocklock);
            rtc_settime(&tp);
            //} else {
            //    return -EPERM;
            //}
            break;
    }
    return ret;
}

#if 0

// SoCのtimerモジュールを使う : GICに割り込みが届かなかったので使わない
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

}

void clock_reset(void)
{
    put32(TMR0_CTRL_REG, get32(TMR0_CTRL_REG) | TMR_RELOAD);
    while(get32(TMR0_CTRL_REG) & TMR_RELOAD);
}

static uint64_t jiffies2 = 0;

/*
 * TODO: tickを進める
 */
void clock_intr(void *param)
{
    // pendingビットをクリア
    //put32(TMR_IRQ_STA_REG, 0x1);
    put32(HS_TMR_STAS_REG, 0x1);
    if ((++jiffies2 % 100) == 0)
        cprintf("&");
    // 時刻更新処理
    //update_proc_time();
}

#endif



