#include <types.h>
#include <param.h>
#include <mmu.h>
#include <sched.h>
#include <spinlock.h>
#include <arm_timer.h>
#include <string.h>
#include <console.h>
#include <vectors.h>
#include <vm.h>
#include <file.h>
#include <fs.h>
#include <mm.h>
#include <linux/wait.h>
#include <linux/errno.h>

// ２種類のタイマーで計時とスケジューラを駆動する（どちらも1tick=10ms）
//   - コアごとにあるARM汎用物理タイマー: スケジューラtickとして使用
//   - コア0のAARM汎用仮想タイマー: 計時tickとして使用

static struct mm_struct mm_table[NR_MMS];

static char *states[] = {
    [TASK_UNUSED]   = "UNUSED  ",
    [TASK_EMBRYO]   = "EMBRYO  ",
    [TASK_RUNNABLE] = "RUNNABLE",
    [TASK_RUNNING]  = "RUNNING ",
    [TASK_SLEEPING] = "SLEEPING",
    [TASK_ZOMBIE]   = "ZOMBIE  "
};

// kernel_stacks[i]: pid=iのタスクのカーネルスタック
static __attribute__ ((aligned (PAGE_SIZE))) char kernel_stacks[NR_TASKS][THREAD_SIZE];

// ブート時に使用され、起動後はidleタスクのカーネルスタックとして使用される
__attribute__ ((aligned (PAGE_SIZE))) char boot_stacks[NCPU][THREAD_SIZE];

struct task_struct *init_task;          // initタスク
struct task_struct *tasks[NR_TASKS];    // 通常タスク
struct task_struct *idle_tasks[NCPU];   // cpuごとにあるidleタスク

// tasks[]の操作を保護する
struct spinlock sched_lock = { .locked=0, .cpu=0, .name="sched" };

// cpu配列
struct cpu cpus[NCPU];

// 実行中のタスクを返す
struct task_struct *myproc(void)
{
    struct task_struct *p;

    // 割り込みを無効にする必要がある
    push_off();
    p = mycpu()->proc;
    pop_off();
    return p;
}

// タスクカーネルスタックの先頭にあるタスクの保存レジスタ("トラップフレーム")を取得する
struct trapframe *task_pt_regs(struct task_struct *task)
{
    uint64_t p = (uint64_t)task + THREAD_SIZE - sizeof(struct trapframe);
    return (struct trapframe *)p;
}

extern void init(int arg);

// schedule()またはtimertick()が実行される前に呼び出す必要がある
void sched_init(void)
{
    // タスクテーブルの初期化
    for (int i = 0; i < NR_TASKS; i++) {
        tasks[i] = (struct task_struct *)(&kernel_stacks[i][0]);
        assert((uint64_t)tasks[i] & ~PAGE_MASK);
        memset(tasks[i], 0, sizeof(struct task_struct));
        initlock(&(tasks[i]->lock), "task");
        tasks[i]->state = TASK_UNUSED;
    }

    // idleテーブルの初期化
    for (int i = 0; i < NCPU; i++) {
        idle_tasks[i] = (struct task_struct *)(&boot_stacks[i][0]);
        cpus[i].proc = idle_tasks[i];
        initlock(&(idle_tasks[i]->lock), "idle");
        snprintf(idle_tasks[i]->name, 10, "idle-%d", i);
        idle_tasks[i]->pid = -1;
    }

    // mm_tableの初期化
    memset(mm_table, 0, sizeof(mm_table));
    for (int i = 0; i < NR_MMS; i++) {
        initlock(&mm_table[i].lock, "mm");
    }

    // initタスク（ユーザプログラム）
    init_task = tasks[0];
    init_task->state = TASK_RUNNABLE;
    init_task->cpu_context.x19 = (uint64_t)init;
    init_task->cpu_context.pc = (uint64_t)ret_from_fork;
    init_task->cpu_context.sp = (uint64_t)init_task + THREAD_SIZE;
    init_task->credits = 0;
    init_task->priority = 2;
    init_task->flags = PF_KTHREAD;
    init_task->mm = 0;
    init_task->killed = 0;
    init_task->pid = 0;
    init_task->cwd = 0;
    safestrcpy(init_task->name, "init", 5);
}

// 現在タスクを実行しているcpuidを返す: みつからない、エラーの場合は -1
static int task_on_cpu(struct task_struct *p)
{
    if (!p) return -1;

    for (int i = 0; i < NCPU; i++) {
        if (cpus[i].proc == p)
            return i;
    }
    return -1;
}

// スケジューラ: 呼び出し元は sched_lockを保持していてはいけない
void schedule(void)
{
    int next, max_cr, cpu, oncpu;

    struct task_struct *p, *cur = myproc();
    int has_runnable;

    acquire(&sched_lock);
    cpu = cpuid();

    while (1) {
        max_cr = -1;
        next = 0;
        has_runnable = 0;

        for (int i = 0; i < NR_TASKS; i++) {
            p = tasks[i];
            assert(p);
            oncpu = task_on_cpu(p);
            // タスクが別のcpuで実行中の場合は触らない
            if (oncpu != -1 && oncpu != cpu)
                continue;
            if ((p == cur && p->state == TASK_RUNNING)
              || p->state == TASK_RUNNABLE) {
                has_runnable = 1;
                if (p->credits > max_cr) {
                    max_cr = p->credits;
                    next = i;
                }
            }
        }
        // 最大クレジットを持つタスクがあったらそれに切り替える
        if (max_cr > 0) {
            switch_to(tasks[next]);
            break;
        }
        // 実行できるタスクがない場合
        if (has_runnable) {
            // 理由1: クレジットが足りない
            for (int i = 0; i < NR_TASKS; i++) {
                p = tasks[i];
                if (p->state != TASK_UNUSED) {
                    p->credits = (p->credits >> 1) + p->priority;
                }
            }
            // 理由2: RUNNABLEな通常タスクがない
        } else {
            switch_to(idle_tasks[cpu]);
            break;

        }
    }
    release(&sched_lock);
}

void leave_scheduler(void) {
    release(&sched_lock);
}

// 自発的スケジュール
void yield(void)
{
    struct task_struct *p = myproc();
    acquire(&sched_lock);
    p->credits = 0;
    release(&sched_lock);
    schedule();
}

// タスク切り替え
// 呼び出し元はsched_lockを保持し、next->lockは保持してはならない
void switch_to(struct task_struct *next)
{
    struct task_struct *prev, *cur;

    cur = myproc();
    assert(cur);
    if (cur == next)
        return;

    prev = cur;
    mycpu()->proc = next;

    if (prev->state == TASK_RUNNING)
        prev->state = TASK_RUNNABLE;
    next->state = TASK_RUNNING;

    // nextタスクがユーザタスクの場合
    if (next->mm) {
        acquire(&next->mm->lock);
        set_pgd(next->mm->pgd);
        release(&next->mm->lock);
    }
    // 実際に切り替え
    cpu_switch_to(prev, next);
}

#define CPU_UTIL_INTERVAL   10      // cpu利用統計の測定頻度

void timer_tick(void)
{
    struct task_struct *cur = myproc();
    struct cpu *cpu = mycpu();

    if (cur) {
        if (cur->pid >= 0 && cur->state == TASK_RUNNING)
            cpu->busy++;

        if ((cpu->total++ % CPU_UTIL_INTERVAL) == CPU_UTIL_INTERVAL - 1) {
            cpu->last_util = cpu->busy * 100 / cpu->total;
            debug("u: %d, b: %d, t: %llu", cpu->last_util, cpu->busy, cpu->total);
        }

        acquire(&sched_lock);
        if (cur->pid >= 0 && --cur->credits > 0) {
            release(&sched_lock);
            return;
        }
        // プリエンプション
        cur->credits = 0;
        release(&sched_lock);
    }

    enable_irq();
    schedule();
    disable_irq();
}

// chanでスリープ中のすべてのプロセスを起床させる。
//  p->stateのみを変更し、
//  schedule()は呼び出さない。起床されられたタスクの数を返す。
//  呼び出し元はsched_lockを保持している必要がある
static int wakeup_nolock(void *chan)
{
    struct task_struct *p;
    int cnt = 0;

    for (int i = 0; i < NR_TASKS; i++) {
        p = tasks[i];
        if (p->state == TASK_UNUSED) continue;
        if (p->state == TASK_SLEEPING && p->chan == chan) {
            p->state = TASK_RUNNABLE;
            cnt++;
            debug("wakeup cpu%d chan=0x%lx pid %d", cpuid(), (uint64_t)p->chan, p->pid);
        }
    }
    return cnt;
}

// chanでsleepしているタスクを起床させる
//  sched_lockをロックせずに呼び出す必要がある
//  割り込み（多くのドライバ）またはタスクから呼び出される
//  起床させられたタスクの数を返す
int wakeup(void *chan)
{
    int cnt;
    acquire(&sched_lock);
    cnt = wakeup_nolock(chan);
    release(&sched_lock);
    return cnt;
}

// chanでスリープする.
// @lk を保持したタスクにより呼び出される
void sleep(void *chan, struct spinlock *lk)
{
    struct task_struct *p = myproc();

    if (lk != &sched_lock) {
        acquire(&sched_lock);
        release(lk);
    }

    p->chan = chan;
    p->state = TASK_SLEEPING;
    p->credits--;

    struct task_struct *idle = idle_tasks[cpuid()];
    mycpu()->proc = idle;
    cpu_switch_to(p, idle);

    p->chan = 0;

    if (lk != &sched_lock) {
        release(&sched_lock);
        acquire(lk);
    }
}

// pが見捨てる子供をinitにわたす（直接、initプロセスにペアリングする）
//  リペア連とした子どもの数を返す
//  呼び出し元はsched_lock を保持していなければならない
static int reparent(struct task_struct *p)
{
    struct task_struct **child;
    int cnt = 0;

    for (child = tasks; child < &tasks[NR_TASKS]; child++) {
        assert(*child);
        if ((*child)->state == TASK_UNUSED) continue;
        if ((*child)->parent == p) {
            (*child)->parent = init_task;
            cnt++;
        }
    }
    return cnt;
}

static void freeproc(struct task_struct *p);

#if 0
// 子プロセスがexitするのを待機して、そのpidを返す。
// このプロエスが子を持たない場合は -1 を返す。
// addr=0 はstatusを考慮しない特別な場合
int wait(uint64_t addr)
{

    struct task_struct **pp;
    int havekids, pid;
    struct task_struct *p = myproc();

    acquire(&sched_lock);

    for (;;) {
        havekids = 0;
        for (pp = tasks; pp < &tasks[NR_TASKS]; pp++) {
            struct task_struct *p0 = *pp;
            assert(p0);
            if (p0->state == TASK_UNUSED) continue;
            if (p0->parent == p) {
                havekids = 1;
                if (p0->state == TASK_ZOMBIE) {
                    pid = p0->pid;
                    assert(addr == 0 || p->mm);
                    if (addr != 0 && copyout(p->mm, addr, (char *)&(p0->xstate), sizeof(p0->xstate)) < 0) {
                        release(&sched_lock);
                        return -1;
                    }
                    freeproc(p0);
                    release(&sched_lock);
                    return pid;
                }
            }
        }

        if (!havekids || killed(p)) {
            release(&sched_lock);
            return -1;
        }

        sleep(p, &sched_lock);
    }
}
#endif

int wait4(pid_t pid, int *status, int options, struct rusage *ru)
{
    struct task_struct **pp;
    int havekids;
    struct task_struct *p = myproc();

    acquire(&sched_lock);

    for (;;) {
        havekids = 0;
        for (pp = tasks; pp < &tasks[NR_TASKS]; pp++) {
            struct task_struct *p0 = *pp;
            assert(p0);
            if (p0->state == TASK_UNUSED || p0->parent != p) continue;
            // TODO: p->pgidを作成したら条件を増やす
            if (pid > 0) {
                if (p->pid != pid)
                    continue;
            }
            havekids = 1;
            if (p0->state == TASK_ZOMBIE
             || (options & WUNTRACED && p0->state == TASK_SLEEPING)
             || (options & WNOHANG)) {
                int status = p->xstate << 8;
                if (status && copyout(p->mm, (uint64_t)status, (char *)&status, sizeof(int)) < 0) {
                    release(&sched_lock);
                    return -EINVAL;
                }
                if (ru && copyout(p->mm, (uint64_t)ru, 0, sizeof(struct rusage)) < 0) {
                    release(&sched_lock);
                    return -EINVAL;
                }
                pid = p0->pid;
                freeproc(p0);
                release(&sched_lock);
                return pid;
            }
        }
        if (!havekids || killed(p)) {
            release(&sched_lock);
            return -ECHILD;
        }
        sleep(p, &sched_lock);
    }
}


// ゾンビタスクとなり、親タスクがwait()を正常に呼び出した場合にのみ、そのタスクから
// CPUが切り離される
void exit_process(int status)
{
    struct task_struct *p = myproc();

    if (p == init_task)
        panic("init exiting");

    for (int fd = 0; fd < NOFILE; fd++) {
        struct file *f = p->ofile[fd];
        fileclose(f);
        p->ofile[fd] = 0;
    }

    if (p->cwd)
        iput(p->cwd);
    p->cwd = 0;

    acquire(&sched_lock);

    if (reparent(p))
        wakeup_nolock(init_task);

    wakeup_nolock(p->parent);
    p->xstate = status;
    p->state = TASK_ZOMBIE;

    struct task_struct *idle = idle_tasks[cpuid()];
    mycpu()->proc = idle;
    cpu_switch_to(p, idle);

    panic("zombie exit");
}

// タスクを破棄する
// sched_lockとp->lockを保持している必要がある
static void freeproc(struct task_struct*p)
{
    assert(p);

    p->state = TASK_UNUSED;

    if (p->mm) {
        acquire(&p->mm->lock);
        int newref = __atomic_sub_fetch(&p->mm->ref, 1, __ATOMIC_SEQ_CST);
        assert(newref >= 0);
        if (newref == 0) {
            free_task_pages(p->mm, 0);
        }
        release(&p->mm->lock);
        p->mm = 0;
    }

    p->flags = 0;
    p->killed = 0;
    p->credits = 0;
    p->chan = 0;
    p->pid = 0;
    p->xstate = 0;
}

// 指定されたPIDのプロセスを終了させる
int kill(int pid)
{
    int i;
    struct task_struct *p;

    acquire(&sched_lock);
    for (i = 0; i < NR_TASKS; i++) {
        p = tasks[i];
        assert(p);
        assert(i != pid || p->state != TASK_UNUSED);
        if (p->state == TASK_UNUSED) continue;
        if (i == pid) {
            acquire(&p->lock);
            p->killed = 1;
            release(&p->lock);
            if (p->state == TASK_SLEEPING) {
                p->state = TASK_RUNNABLE;
            }
            release(&sched_lock);
            return 0;
        }
    }
    release(&sched_lock);
    return -1;
}

// （不具合のあったタスクなどの）タスクを"killed"とマークする
void setkilled(struct task_struct *p)
{
    acquire(&p->lock);
    p->killed = 1;
    release(&p->lock);
}

// タスクはkillされているか
int killed(struct task_struct *p)
{
    int k;
    acquire(&p->lock);
    k = p->killed;
    release(&p->lock);
    return k;
}

// プロセス一覧をコンソールに出力する。デバッグ用。
// ユーザがコンソールで^Pをタイプすることで実行される
// 動かない状態をさらに悪化させないよう、ロックしていない.
void procdump(void) {
    struct task_struct *p;
    char *state;

    cprintf("\t %5s %10s %10s %20s", "pid", "state", "name", "sleep-on");

    for (int i = 0; i < NR_TASKS; i++) {
        p = tasks[i];
        if (p->state == TASK_UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        cprintf("\t %5d %10s %10s %20llx\n", p->pid, state, p->name, (uint64_t)p->chan);
    }

    info("paging mem: used %u total %u (%u/100)\n",
        paging_pages_used, PAGE_NUM,
        paging_pages_used*100/(PAGE_NUM));

    // debug
    info("cpu total ticks: %llu %llu %llu %llu\n",
         cpus[0].total, cpus[1].total, cpus[2].total, cpus[3].total);
    info("cpu busy ticks: %d %d %d %d\n",
         cpus[0].busy, cpus[1].busy, cpus[2].busy, cpus[3].busy);
}

//============= fork関係 ================================================

// 空のmmを割り当てる
static struct mm_struct *alloc_mm(void) {
    struct mm_struct *mm;

    for (mm = mm_table; mm < mm_table + NR_MMS; mm++) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&mm->ref, &expected, 1, 0,
                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            acquire(&mm->lock);
            break;
        }
    }

    if (mm == mm_table + NR_MMS) {
        error("no free mm");
        return 0;
    }

    mm->pgd = 0;
    mm->kernel_pages_count = 0;
    mm->user_pages_count = 0;
    return mm;
}

// カーネルタスクをEL0に昇格させて、最初のユーザータスクを作成する
int move_to_user_mode(uint64_t start, uint64_t size, uint64_t pc)
{
    assert(size <= PAGE_SIZE);

    struct task_struct *cur = myproc();
    struct trapframe *regs = task_pt_regs(cur);
    assert(!cur->mm);

    if (!(cur->mm = alloc_mm()))
        return -1;

    regs->pstate = PSR_MODE_EL0t;
    regs->pc = pc;
    regs->sp = USER_TOP;

    void *code_page = allocate_user_page_mm(cur->mm, 0, MMU_PTE_FLAGS | MM_AP_RW);
    if (code_page == 0) {
        release(&cur->mm->lock);
        return -1;
    }

    cur->mm->sz = cur->mm->codesz = PAGE_SIZE;
    memmove(code_page, (void *)start, size);
    set_pgd(cur->mm->pgd);
    safestrcpy(cur->name, "initusr", sizeof(cur->name));
    cur->cwd = namei("/", AT_FDCWD);
    assert(cur->cwd);
    release(&cur->mm->lock);

    fsinit(ROOTDEV);
#ifdef CONFIG_FAT
    fsinit(SECONDDEV);
#endif

    return 0;
}

static int lastpid = 0;

// タスク（ユーザタスクとカーネルタスク）を作成する
int copy_process(uint64_t clone_flags, uint64_t fn, uint64_t arg, const char *name)
{
    struct task_struct *p = 0, *cur = myproc();
    int i, pid;

    acquire(&sched_lock);
    for (i = 0; i < NR_TASKS; i++) {
        pid = (lastpid+i+i) % NR_TASKS;
        p = tasks[pid];
        assert(p);
        if (p->state == TASK_UNUSED) {
            lastpid = pid;
            break;
        }
    }
    if (i == NR_TASKS) {
        release(&sched_lock);
        return -1;
    }

    memset(p, 0, sizeof(struct task_struct));
    initlock(&p->lock, "proc");

    acquire(&p->lock);
    acquire(&cur->lock);

    struct trapframe *childregs = task_pt_regs(p);

    // カーネルスレッド
    if (clone_flags == PF_KTHREAD) {
        p->cpu_context.x19 = fn;
        p->cpu_context.x20 = arg;
    } else {
        struct trapframe *cur_regs = task_pt_regs(cur);
        *childregs = *cur_regs;
        childregs->regs[0] = 0;     // fork()の子側のリターン値
        // ユーザスレッド
        if (clone_flags & PF_UTHREAD) {
            p->mm = cur->mm;
            __atomic_add_fetch(&p->mm->ref, 1, __ATOMIC_SEQ_CST);
            childregs->sp = arg;
        // ユーザプロセス
        } else {
            struct mm_struct *mm = alloc_mm();
            if (!mm) {
                error("no mm");
                return -1;
            }
            p->mm = mm;
            dup_current_virt_memory(mm);
            release(&mm->lock);
        }

        for (int i = 0; i < NOFILE; i++) {
            if (cur->ofile[i])
                p->ofile[i] = filedup(cur->ofile[i]);
        }
        p->cwd = idup(cur->cwd);
    }

    if (name)
        safestrcpy(p->name, name, sizeof(p->name));
    else
        safestrcpy(p->name, cur->name, sizeof(cur->name));

    p->flags = clone_flags;
    p->credits = p->priority = cur->priority;
    p->pid = pid;

    p->cpu_context.pc = (uint64_t)ret_from_fork;
    p->cpu_context.sp = (uint64_t)childregs;
    p->cpu_context.fp = (uint64_t)childregs->stackframe;

    release(&cur->lock);
    release(&p->lock);

    p->parent = cur;
    p->state = TASK_RUNNABLE;

    release(&sched_lock);

    return pid;
}
