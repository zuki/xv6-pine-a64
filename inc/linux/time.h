#ifndef INC_LINUX_TIME_H
#define INC_LINUX_TIME_H

#ifndef __ASSEMBLER__

#include <types.h>
#include <list.h>

// 1 tick = 10 ms
#define HZ                  100
#define MSEC2HZ(msec)       ((msec) * HZ / 1000)
#define TICK_USEC           (10000UL)
#define TICK_NSEC           (10000000UL)

typedef void(*fn)(uint64_t);

struct timeval {
    time_t      tv_sec;     /* 秒 */
    suseconds_t tv_usec;    /* マイクロ秒 */
};

struct timespec {
    time_t  tv_sec;         /* 秒 */
    long    tv_nsec;        /* ナノ秒 */
};

struct itimerval {
    struct timeval it_interval; /* 周期的タイマーのInterval */
    struct timeval it_value;    /* 次の時間切れまでの時間 */
};

struct timer_list {
    struct list_head list;
    uint64_t expires;
    uint64_t data;
    void (*fn)(uint64_t);
};

extern uint64_t jiffies;
extern struct timespec xtime;

#define CURRENT_TIME (xtime.tv_sec)

#define CLOCK_REALTIME           0  /* 実時間を計測する設定可能なシステム全体で一意な時間 */
#define CLOCK_MONOTONIC          1  /* ある開始時点から単調増加するクロック */
#define CLOCK_PROCESS_CPUTIME_ID 2  /* プロセス単位の CPU タイムクロック */
#define CLOCK_THREAD_CPUTIME_ID  3  /* スレッド固有の CPU タイムクロック */
#define CLOCK_MONOTONIC_RAW      4  /* NTPの調整やadjtime(3)の段階的な調整の影響を受けないCLOCK_MONOTONIC */
#define CLOCK_REALTIME_COARSE    5  /* 高速だが精度が低い CLOCK_REALTIME */
#define CLOCK_MONOTONIC_COARSE   6  /* 高速だが精度が低い CLOCK_MONOTONIC */
#define CLOCK_BOOTTIME           7  /* システムがサスペンドされている時間も含まれるCLOCK_MONOTONIC */
#define CLOCK_REALTIME_ALARM     8  /* 実時間を計測する設定不能なシステム全体で一意な時間 */
#define CLOCK_BOOTTIME_ALARM     9
#define CLOCK_SGI_CYCLE         10
#define CLOCK_TAI               11

// タイマー種別
#define ITIMER_REAL             0   /* 実時間タイマー: SIGALRM */
#define ITIMER_VIRTUAL          1   /* プロセスタイマー: SIGVTARM */
#define ITIMER_PROF             2   /* プロファイラ用タイマー: SIGPROF */

#define TIME_UTC                1

// タイマーの登録
extern void add_timer(struct timer_list *timer);
// タイマーのキャンセル
extern int del_timer(struct timer_list *timer, boolean del);
// すでに実行中の場合は終了を待つ
#define del_timer_sync(t, d)    del_timer(t, d);
#define sync_timers()       do { } while (0)

// expireの変更
int mod_timer(struct timer_list *timer, uint64_t expires);

static inline void init_timer(struct timer_list * timer)
{
	timer->list.next = timer->list.prev = NULL;
}

static inline int timer_pending(const struct timer_list * timer)
{
	return timer->list.next != NULL;
}

#define time_after(a,b)     ((int64_t)(b) - (int64_t)(a) < 0)
#define time_before(a,b)    time_after(b,a)

#define time_after_eq(a,b)  ((int64_t)(a) - (int64_t)(b) >= 0)
#define time_before_eq(a,b) time_after_eq(b,a)

void init_timervecs(void);
void run_timer_list(void);
long getitimer(int, struct itimerval *);
void it_real_fn(uint64_t);
long setitimer(int, struct itimerval *, struct itimerval *);

#define timercmp(a, b, cmp) \
    ((a)->tv_sec == (b)->tv_sec ? (a)->tv_usec cmp (b)->tv_usec : (a)->tv_sec cmp (b)->tv_sec)

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

#endif

#endif
