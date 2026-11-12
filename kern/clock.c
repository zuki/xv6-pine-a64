#include <types.h>
#include <clock.h>
#include <sched.h>
#include <vm.h>
#include <timer.h>
#include <linux/time.h>
#include <arm.h>
#include <irq.h>
#include <arm_timer.h>
#include <rtc.h>
#include <console.h>
#include <spinlock.h>
#include <linux/errno.h>

// arm汎用仮想タイマーを計時に使用する

// 1 tick = 10ms で増えていく
uint64_t jiffies = 0;
/* 現在時刻 (wall_time) */
struct timespec xtime  __attribute__ ((aligned (16)));
/* 直近のwall_time更新時のjiffies */
uint64_t wall_jiffies = 0;
/* jiffies, xtime, wall_jiffies を保護 */
struct spinlock clocklock;

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
    irq_register(IRQ_ARM_VTIMER, clock_intr, 0);
    irq_enable(IRQ_ARM_VTIMER);

    initlock(&clocklock, "clock");
    if (rtc_gettime(&xtime) < 0) {
        xtime.tv_nsec = 0L;
        xtime.tv_sec = 1790474684;  // 2026-09-27 11:04:44
    }

    info("clock_init ok");
}

void clock_intr(void *param)
{
    acquire(&clocklock);
    // pendingビットをクリア
    arm_vtimer_reset();
    jiffies++;
    update_times();
    //wakeup(&jiffies);
    release(&clocklock);
    //run_timer_list();
}

int clock_gettime(clockid_t clk_id, uint64_t tpp, int user)
{
    struct timespec tp;
    int ret = 0;

    switch(clk_id) {
        default:
            ret = -EINVAL;
        case CLOCK_REALTIME:
            acquire(&clocklock);
            tp.tv_nsec = xtime.tv_nsec;
            tp.tv_sec = xtime.tv_sec;
            release(&clocklock);
            if (user)
                ret = copyout(myproc()->mm, tpp, (char *)&tp, sizeof(tp));
            else
                memmove((char *)tpp, (const char *)&tp, sizeof(tp));
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

    return ret;
}

int clock_settime(clockid_t clk_id, uint64_t tpp)
{
    struct timespec tp;
    int ret = 0;

    switch(clk_id) {
        default:
            ret = -EINVAL;
        case CLOCK_REALTIME:
            //if (capable(CAP_SYS_TIME)) {
            if ((ret = copyin(myproc()->mm, (char *)&tp, tpp, sizeof(tp))) < 0)
                break;
            acquire(&clocklock);
            xtime.tv_nsec = tp.tv_nsec;
            xtime.tv_sec = tp.tv_sec;
            release(&clocklock);
            rtc_settime(&tp);
            break;
    }
    return ret;
}

