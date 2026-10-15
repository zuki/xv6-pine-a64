#include <types.h>
#include <timer.h>
#include <linux/time.h>
#include <arm_timer.h>
#include <arm.h>
#include <irq.h>
#include <console.h>
#include <param.h>
#include <spinlock.h>

/* timer_listを保護するロック */
struct spinlock timerlock;
/* タイマーリスト: コアごとに存在 */
static struct timer_list timer_list;
/* 次のタイマー発火までのticks */
static uint64_t timer_jiffies = 0;

//static volatile uint64_t core_jiffies2[NCPU] = { 0 };
//static int core_ids[NCPU] = {0, 1, 2, 3};

// 物理タイマーをタイマーとして使用する: すべてのコアで実行
void timer_init(int core)
{
    arm_ptimer_init(INTERVAL);
    irq_register(GIC_PPI(14), timer_intr, NULL);
    irq_enable(GIC_PPI(14));

    initlock(&timerlock, "timer");

    info("timer_init [%d]: arg: %p", core, &core);
}

void timer_intr(void *param)
{

    // pendingビットをクリア
    arm_ptimer_reset(INTERVAL);
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

void delay_ms(uint32_t ms)
{
    delay(CYCLEPERMS * ms);
}

void delay_us(uint32_t us)
{
    delay(CYCLEPERUS * us);
}

/* タイマーリストに追加し、発火時間を再計算する */
static void internal_add_timer(struct timer_list *timer)
{
    //割り込み禁止（trap.c）&& timerlockを保持

    struct list_head *head, *curr;
    struct timer_list *entry;
    boolean unset = true;

    head = &timer_list.list;
    curr = head->next;

    while (curr != head) {
        entry = list_entry(curr, struct timer_list, list);
        if (timer->expires <= entry->expires) {
            if (curr->prev == head) {           // entryは先頭
                entry->expires -= timer->expires;
                list_push_front(&timer_list.list, &timer->list);
            } else if (curr->next == head) {    // entryは末尾
                timer->expires -= entry->expires;
                list_push_back(&timer_list.list, &timer->list);
            } else {                            // entryの前に挿入
                entry->expires -= timer->expires;
                list_insert(&timer->list, entry->list.prev, &entry->list);
            }
            unset = false;
            break;
        }
        timer->expires -= entry->expires;
        curr = curr->next;
    }
    if (unset)
        list_push_back(&timer_list.list, &timer->list);
}

// TODO
struct timer_list *alloc_timer(void)
{
#if 0
    struct timer_list *timer = (struct timer_list *)slab_cache_alloc(TIMERS);
    memset(timer, 0, sizeof(struct timer_list));
    timer->list.next = timer->list.prev = NULL;
    return timer;
#endif

    return NULL;
}

// TODO
void free_timer(struct timer_list *timer) {
    //slab_cache_free(TIMERS, timer);
}

/* タイマーを登録から外す */
static void detach_timer(struct timer_list *timer)
{
    if (!timer_pending(timer))  // 未登録
        return;
    list_drop(&timer->list);    // リストから自分を外す
}

/* タイマーを追加する */
void add_timer(struct timer_list *timer)
{
    acquire(&timerlock);
    if (timer_pending(timer))   // 二重登録
        goto bug;
    internal_add_timer(timer);
    release(&timerlock);
    return;
bug:
    release(&timerlock);
    warn("bug: kernel timer added twice at %p",
            __builtin_return_address(0));
}

/* タイマーを削除する. del=true: timer->data, timerを削除, proc.real_timerは削除しない */
int del_timer(struct timer_list * timer, boolean del)
{
    acquire(&timerlock);
    detach_timer(timer);                      // 1. リストから削除
    timer->list.next = timer->list.prev = NULL;     // 2. 内部リストからクリア
    release(&timerlock);
    if (del) {
        if (timer->data != 0) {
            //kmfree((void *)timer->data);
        }
        free_timer(timer);
    }
    return 0;
}

/* 時限が来たタイマーを実行した後、タイマーリストを更新する */
void run_timer_list(void)
{
    struct list_head *head, *curr;
    uint64_t delta = jiffies - timer_jiffies;

    acquire(&timerlock);
    head = &timer_list.list;
    curr = head->next;

    while (curr != head) {
        struct timer_list *timer;
        void (*fn)(uint64_t);
        uint64_t data;              // データには(void *)が設定可能

        timer = list_entry(curr, struct timer_list, list);
        if (timer->expires <= delta) {
            fn = timer->fn;
            data = timer->data;
            detach_timer(timer);
            timer->list.next = timer->list.prev = NULL;
            release(&timerlock);
            fn(data);
            acquire(&timerlock);
        } else {
            timer->expires -= delta;    // リストの先頭のexpiresを経過時刻だけ減ずる
            break;
        }
        curr = curr->next;
    }
    timer_jiffies += delta;
    release(&timerlock);
}
