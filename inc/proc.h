#ifndef INC_PROC_H
#define INC_PROC_H

#include <types.h>
#include <arm.h>
#include <mmu.h>
#include <trap.h>
#include <spinlock.h>
#include <list.h>
#include <slab.h>

#define NPROC           100
#define NCPU            4
#define NOFILE          16      // Open files per process
#define THREAD_SIZE     PGSIZE      // タスクごとのカーネルスタックサイズ

/* Stack must always be 16 bytes aligned. */
struct context {
    uint64_t lr0, lr, fp;
    uint64_t x[10];             /* X28 ... X19 */
    uint64_t padding;
    uint64_t q0[2];             /* V0 */
};

extern struct slab_cache *VMA;

// vma (virtual memory map area)構造体 */
struct vma {
    void *          addr;       /* 先頭アドレス */
    size_t          length;     /* 領域長 */
    int             prot;       /* vma属性 */
    int             flags;      /* vmaフラグ */
    struct file *   f;          /* mapしているファイル */
    off_t           offset;     /* この領域のファイル内のオフセット */
    struct vma *    next;       /* 次のvmaへのポインタ */
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

/* Per-process state */
struct proc {
    /*
     * Memory layout
     *
     * +----------+
     * |  Kernel  |
     * +----------+  KERN_BASE
     * |  Stack   |
     * +----------+  KERN_BASE - stksz
     * |   ....   |
     * |   ....   |
     * +----------+  base + sz
     * |   Heap   |
     * +----------+
     * |   Code   |
     * +----------+  base
     * | Reserved |
     * +----------+  0
     *
     */
    size_t base, sz;
    size_t stksz;

    void *pgdir;                /* User space page table. */
    void *kstack;               /* Bottom of kernel stack for this process. */
    enum procstate state;       /* Process state. */
    int pid;                    /* Process ID. */

    struct proc *parent;        /* Parent process */
    struct list_head child;     /* Child list of this process. */
    struct list_head clink;     /* Child list of this process. */

    struct trapframe *tf;       /* Trap frame for current syscall. */
    struct context *context;    /* swtch() here to run process. */
    struct list_head link;      /* linked list of running process. */

    void *chan;                 /* If non-zero, sleeping on chan */
    int killed;                 // If non-zero, have been killed

    struct file *ofile[NOFILE]; // Open files

    struct inode *cwd;          // Current directory
    char name[16];              // Process name (debugging)

    struct vma  *vmas;          // vma領域のリストの先頭のポインタ
};

/* Per-CPU state */
struct cpu {
    struct context *scheduler;  /* swtch() here to enter scheduler */
    struct proc *proc;          /* The process running on this cpu or null. */
    struct proc *idle;          /* The idle process. */
    volatile int started;       /* Has the CPU started? */
    //struct spinlock lock;
    //int     noff;               // push_off() ネストの深さ
    //int     intena;             // push_off()する前に割り込みが有効だったか
};

extern struct cpu cpu[NCPU];

static inline struct cpu *
thiscpu()
{
    return &cpu[cpuid()];
}

static inline struct proc *thisproc()
{
#if 0
    struct proc *p;

    push_off();
    p = thiscpu()->proc;
    pop_off();
    return p;
#endif
    return thiscpu()->proc;
}

void proc_init();
void user_init();
void scheduler();
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);
void yield();
void exit(int);
int  wait();
int  fork();

struct file *get_fd(struct proc *p, int fd);

void procdump();

#endif
