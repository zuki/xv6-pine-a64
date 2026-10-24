#ifndef H_SCHED_H
#define H_SCHED_H

#define THREAD_CPU_CONTEXT      0       // task_structにおけるcpu_contextのオフセット

#ifndef __ASSEMBLER__
#include <types.h>
#include <arm.h>
#include <linux/time.h>
#include <mmu.h>
#include <spinlock.h>
#include <irq.h>
#include <utils.h>
#include <linux/resources.h>

#define THREAD_SIZE     PAGE_SIZE          // タスクごとのカーネルスタックサイズ

// タスクの状態フラグ
#define TASK_UNUSED     0
#define TASK_EMBRYO     1
#define TASK_RUNNABLE   2
#define TASK_RUNNING    3
#define TASK_SLEEPING   4
#define TASK_ZOMBIE     5

#define PF_KTHREAD      0x2             // カーネルスレッド
#define PF_UTHREAD      0x4             // ユーザスレッド

// タスクコンテキスト: スケジューリング時のコンテキストスイッチ用
// x0-x7は関数呼び出しの引数で、x9-x15はcaller save.
// x19-x29がcallee save
struct cpu_context {
    uint64_t    x19;
    uint64_t    x20;
    uint64_t    x21;
    uint64_t    x22;
    uint64_t    x23;
    uint64_t    x24;
    uint64_t    x25;
    uint64_t    x26;
    uint64_t    x27;
    uint64_t    x28;
    uint64_t    fp;
    uint64_t    sp;
    uint64_t    pc;
}; // 13 regs

struct user_page {
    uint64_t    phys_addr;
    uint64_t    virt_addr;
};

struct trapframe {
    uint64_t regs[31];
    uint64_t sp, pc, pstate;
    uint64_t stackframe[2];     // 巻き戻し用
};

// ユーザタスクのVM: VMはユーザタスク間で共有される場合がある。
// カーネルスレッドでは使用されない
// TODO: kernel_pagesはlistにしたほうが良い
struct mm_struct {
    int ref;                    // このmm_structを参照しているtask_structsの数
    struct spinlock lock;       // 以下のすべてのフィールドを保護する
    uint64_t pgd;               // 物理アドレス。ttbr0 (user va)にロードされる
    uint64_t sz;                // ファイルサイズ: ユーザタスク用。VA
    uint64_t codesz;            // コードサイズ: ユーザタスク用。VA
    int user_pages_count;       // このタスクで使われているユーザページ数
    int kernel_pages_count;     // このタスクで使われているカーネルページ数
    uint64_t kernel_pages[MAX_TASK_KER_PAGES];  // カーネルページ. PA　pgtable用など
};

// タスクを記述する構造体
struct task_struct {
    // このタスクのプライベートデータ。task->lockは不要
    struct cpu_context cpu_context; // 最初にある必要がある。レジスタ値
    struct file *ofile[NOFILE];     // Openしているファイル
    struct inode *cwd;              // カレントディレクトリ
    char name[16];                  // タスク名 (デバッグ用)
    struct mm_struct *mm;           // UTでは共有あり、KTはなし
    uint64_t flags;
    long preempt_count; // cf: preempt_enable()  TO DELETE

    struct spinlock lock;           // 以下の2つのメンバを保護する
    int killed;                     // 非0の場合、killedされている.
    int pid;                        // プロセスID

    // 以下のメンバを保護するためにグローバルなsched_lockが必要。
    int state;                  // タスクの状態
    long credits;               // scheduleの「クレジット」。残りticks
    long priority;              // デフォルト優先度
    int xstate;                 // 親のwaitに返すExit状態
    void *chan;                 // 非ゼロの場合、chanでsleepしている
    struct task_struct *parent; // 親プロセス
};

// ページのbottom half; top halfはカーネルスタック用に十分なスペースを残していること
_Static_assert(sizeof(struct task_struct) < 1200);

#define MEMBER_OFFSET(type, member) ((long)(&((type *)0)->member))
// task_struct::killed
#define TASK_STRUCT_KILLED_OFFSET_C MEMBER_OFFSET(struct task_struct, killed)

#define S_STACKFRAME_C MEMBER_OFFSET(struct trampframe, stackframe)
#define S_FRAME_SIZE_C  sizeof(struct trapframe)

//=============== CORE (cpu) 関連 ====================

struct cpu {
    struct task_struct *proc;   // このcpuで実行中のプロセス
    int noff;               // push_off() ネストの深さ.
    int intena;             // push_off()する前に割り込みが有効だったか
    int busy;               // 現在の測定期間中にビジーだったtick数（統計用）
    int last_util;          // 直前の想定期間におけるcpuの稼働率 (%)統計用）
    uint64_t total;         // cpu起動以後のtick数
};

extern struct cpu cpus[NCPU];
static inline struct cpu *mycpu(void) {
    return &cpus[cpuid()];
}

// --------------- fork related ----------------------- //
#define PSR_MODE_EL0t	0x00000000
#define PSR_MODE_EL1t	0x00000004
#define PSR_MODE_EL1h	0x00000005
#define PSR_MODE_EL2t	0x00000008
#define PSR_MODE_EL2h	0x00000009
#define PSR_MODE_EL3t	0x0000000c
#define PSR_MODE_EL3h	0x0000000d


#endif  // ! __ASSEMBLER__

#define TASK_STRUCT_KILLED_OFFSET   304     // see above
#define S_X0                        0       // 保存済スタックフレーム内のx0のオフセット
#define S_STACKFRAME                272     // struct tramframeにおけるstackframeのオフセット
#define S_FRAME_SIZE                288     // struct tramframeのサイズ

#ifndef __ASSEMBLER__

_Static_assert(TASK_STRUCT_KILLED_OFFSET == TASK_STRUCT_KILLED_OFFSET_C);
_Static_assert(S_FRAME_SIZE == S_FRAME_SIZE_C);

void sched_init(void);
void schedule(void);
void yield(void);
void leave_scheduler(void);
void switch_to(struct task_struct *next);
void timer_tick(void);
int wakeup(void *chan);
void sleep(void *chan, struct spinlock *lk);
int wait4(int pid, int *status, int options, struct rusage *rusage);
//int wait(uint64_t addr);
void exit_process(int status);
int kill(int pid);
void setkilled(struct task_struct *p);
int  killed(struct task_struct *p);
void procdump(void);
int move_to_user_mode(uint64_t start, uint64_t size, uint64_t pc);
int copy_process(uint64_t clone_flags, uint64_t fn, uint64_t arg, const char *name);

// sched.S
void cpu_switch_to(struct task_struct* prev, struct task_struct* next);

struct task_struct *myproc(void);
struct trapframe *task_pt_regs(struct task_struct *task);

#endif

#endif
