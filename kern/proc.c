#include <types.h>
#include <proc.h>
#include <string.h>
#include <memlayout.h>
#include <list.h>
#include <console.h>
#include <mm.h>
#include <vm.h>
#include <spinlock.h>
#include <cache.h>
#include <sd.h>
#include <file.h>
#include <mmap.h>
#include <slab.h>

extern void trapret();
extern void swtch(struct context **old, struct context *new);

static void forkret();
static void idle_init();

#define SQSIZE  0x100           /* Must be power of 2. */
#define HASH(x) ((((uint64_t)(x)) >> 5) & (SQSIZE - 1))

__attribute__ ((aligned (PGSIZE))) char boot_stacks[NCPU][THREAD_SIZE];

struct cpu cpu[NCPU];

struct {
    struct proc proc[NPROC];
    struct list_head slpque[SQSIZE];
    struct list_head sched_que;
    struct spinlock lock;
} ptable;

struct proc *initproc;
static int pid = 0;
struct slab_cache *VMA;

void proc_init()
{
    list_init(&ptable.sched_que);
    for (int i = 0; i < SQSIZE; i++)
        list_init(&ptable.slpque[i]);

    VMA = slab_cache_create("vma", sizeof(struct vma), 0);
}

// TODO: use kmalloc
/*
 * Look in the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state required to run in the kernel.
 * Otherwise return 0.
 */
static struct proc *
proc_alloc()
{
    struct proc *p;
    int found = 0;

    acquire(&ptable.lock);
    trace("acq ptable");
    for (p = ptable.proc; p < ptable.proc + NPROC; p++) {
        if (p->state == UNUSED) {
            memset(p, 0, sizeof(*p));
            found = 1;
            break;
        }
    }

    if (!found || !(p->kstack = kalloc(1))) {
        release(&ptable.lock);
        trace("rel ptable");
        return 0;
    }

    p->pid = ++pid;
    p->state = EMBRYO;
    release(&ptable.lock);
    trace("rel ptable");

    p->name[0] = 0;

    void *sp = p->kstack + PGSIZE;
    //assert(sizeof(*p->tf) == 18 * 16 && sizeof(*p->context) == 7 * 16);

    sp -= sizeof(*p->tf);
    p->tf = sp;
    /* No user stack for init process. */
    p->tf->spsr = p->tf->sp = 0;

    sp -= sizeof(*p->context);
    p->context = sp;
    p->context->lr0 = (uint64_t) forkret;
    p->context->lr = (uint64_t) trapret;

    list_init(&p->child);

    return p;
}

static struct proc *
proc_initx(char *name, char *code, size_t len)
{
    struct proc *p = proc_alloc();
    void *va = kalloc(1);
    assert(p && va);

    p->pgdir = vm_init();
    assert(p->pgdir);

    int ret = uvm_map(p->pgdir, 0, PGSIZE, V2P(va));
    assert(ret == 0);

    memmove(va, code, len);
    assert(len <= PGSIZE);

    // Flush dcache to memory so that icache can retrieve the correct one.
    __asm_flush_dcache_range(va, va + len);

    p->stksz = 0;
    p->sz = PGSIZE;
    p->base = 0;
    p->vmas = NULL;

    p->tf->elr = 0;

    safestrcpy(p->name, name, sizeof(p->name));
    return p;
}

/* Initialize per-cpu idle process. */
static void
idle_init()
{
    extern char ispin[], eicode[];
    thiscpu()->idle = proc_initx("idle", ispin, (size_t)(eicode - ispin));
}

/* Set up the first user process. */
void
user_init()
{
    extern char icode[], eicode[];
    struct proc *p = proc_initx("icode", icode, (size_t)(eicode - icode));
    p->cwd = namei("/");
    assert(p->cwd);

    acquire(&ptable.lock);
    trace("acq ptable");
    list_push_back(&ptable.sched_que, &p->link);
    release(&ptable.lock);
    trace("rel ptable");
}

/*
 * Per-CPU process scheduler.
 * Each CPU calls scheduler() after setting itself up.
 * Scheduler never returns. It loops, doing:
 * - choose a process to run
 * - swtch to start running that process
 * - eventually that process transfers control
 *   via swtch back to the scheduler.
 */
void
scheduler()
{
    idle_init();
    for (struct proc * p;;) {
        acquire(&ptable.lock);
        trace("acq ptable");
        struct list_head *head = &ptable.sched_que;
        if (list_empty(head)) {
            p = thiscpu()->idle;
        } else {
            p = container_of(list_front(head), struct proc, link);
            list_pop_front(head);
        }
        uvm_switch(p->pgdir);
        thiscpu()->proc = p;
        swtch(&thiscpu()->scheduler, p->context);
        release(&ptable.lock);
        trace("rel ptable");
    }
}

/*
 * A fork child's very first scheduling by scheduler()
 * will swtch here. "Return" to user space.
 */
static void
forkret()
{
    static int first = 1;
    if (first && thisproc() != thiscpu()->idle) {
        first = 0;
        release(&ptable.lock);
        trace("rel ptable");
        fsinit(ROOTDEV);
    } else {
        release(&ptable.lock);
        trace("rel ptable");
    }
    trace("proc '%s'(%d)", thisproc()->name, thisproc()->pid);
}

/* Give up CPU. */
void
yield()
{
    struct proc *p = thisproc();
    acquire(&ptable.lock);
    trace("acq ptable");
    if (p != thiscpu()->idle)
        list_push_back(&ptable.sched_que, &p->link);
    p->state = RUNNABLE;
    swtch(&p->context, thiscpu()->scheduler);
    p->state = RUNNING;
    release(&ptable.lock);
    trace("rel ptable");
}

/*
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void
sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = thisproc();
    int i = HASH(chan);
    assert(i < SQSIZE);
    assert(p != thiscpu()->idle);

    if (lk != &ptable.lock) {
        acquire(&ptable.lock);
        trace("acq ptable");
        release(lk);
        trace("rel %s", lk->name);
    }

    p->chan = chan;
    list_push_back(&ptable.slpque[i], &p->link);

    p->state = SLEEPING;
    trace("'%s'(%d) sleep lk=0x%p", p->name, p->pid, lk);
    swtch(&thisproc()->context, thiscpu()->scheduler);
    trace("'%s'(%d) wakeup lk=0x%p", p->name, p->pid, lk);
    p->state = RUNNING;

    if (lk != &ptable.lock) {
        release(&ptable.lock);
        trace("rel ptable");
        acquire(lk);
        trace("acq %s", lk->name);
    }
}

/*
 * Wake up all processes sleeping on chan.
 * The ptable lock must be held.
 */
static void
wakeup1(void *chan)
{
    struct list_head *q = &ptable.slpque[HASH(chan)];
    struct proc *p, *np;

    list_foreach_safe(p, np, q, link) {
        if (p->chan == chan) {
            trace("wake '%s'(%d)", p->name, p->pid);
            list_drop(&p->link);
            list_push_back(&ptable.sched_que, &p->link);
            p->state = RUNNABLE;
        }
    }
}

/* Wake up all processes sleeping on chan. */
void
wakeup(void *chan)
{
    acquire(&ptable.lock);
    trace("acq ptable");
    wakeup1(chan);
    release(&ptable.lock);
    trace("rel ptable");
}

/*
 * Create a new process copying p as the parent.
 * Sets up stack to return as if from system call.
 * Caller must set state of returned proc to RUNNABLE.
 */
int fork()
{
    int ret = 0;

    struct proc *cp = thisproc();
    struct proc *np = proc_alloc();

    if (np == 0) {
        error("proc_alloc returns null");
        return -1;
    }

    // 親プロセスから子プロセスにvmasをコピーする
    trace("call copy_vmas: p: %d, np: %d", cp->pid, np->pid);
    if ((ret = copy_vmas(cp, np)) < 0) {
        //trace("ret=%d", ret);
        acquire(&ptable.lock);
        np->state = UNUSED;
        release(&ptable.lock);
        error("failed copy_vmas");
        return ret;
    } else {
        //print_mmap_list(cp, "fork cp");
        //print_mmap_list(np, "fork np");
    }

    if ((np->pgdir = uvm_copy(cp->pgdir)) == 0) {
        kfree(np->kstack);

        acquire(&ptable.lock);
        trace("acq ptable");
        np->state = UNUSED;
        release(&ptable.lock);
        trace("rel ptable");
        error("uvm_copy returns null");
        return -1;
    }

    np->parent = cp;

    np->base = cp->base;
    np->sz = cp->sz;
    np->stksz = cp->stksz;

    memmove(np->tf, cp->tf, sizeof(*np->tf));

    // Fork returns 0 in the child.
    np->tf->x[0] = 0;

    for (int i = 0; i < NOFILE; i++)
        if (cp->ofile[i])
            np->ofile[i] = filedup(cp->ofile[i]);
    np->cwd = idup(cp->cwd);

    int pid = np->pid;

    acquire(&ptable.lock);
    trace("acq ptable");
    list_push_back(&cp->child, &np->clink);
    list_push_back(&ptable.sched_que, &np->link);
    np->state = RUNNABLE;
    release(&ptable.lock);
    trace("rel ptable");

    trace("'%s'(%d) fork '%s'(%d)", cp->name, cp->pid, np->name, np->pid);

    return pid;
}


/*
 * Wait for a child process to exit and return its pid.
 * Return -1 if this process has no children.
 */
int
wait()
{
    struct proc *cp = thisproc();

    struct list_head *q = &cp->child;
    struct proc *p, *np;

    acquire(&ptable.lock);
    trace("acq ptable");
    while (!list_empty(q)) {
        list_foreach_safe(p, np, q, clink) {
            if (p->state == ZOMBIE) {
                assert(p->parent == cp);

                list_drop(&p->clink);

                kfree(p->kstack);
                vm_free(p->pgdir);
                p->state = UNUSED;

                int pid = p->pid;
                release(&ptable.lock);
                trace("rel ptable");
                return pid;
            }
        }
        sleep(cp, &ptable.lock);
    }
    release(&ptable.lock);
    trace("rel ptable");
    return -1;
}

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 */
void
exit(int err)
{
    struct proc *cp = thisproc();
    if (cp == initproc)
        panic("init exit");

    if (err) {
        warn("exit: pid %d, err %d", cp->pid, err);
    }

    // vmaを解除する
    //print_mmap_list(p, "before exit");
    if (cp->vmas) {
        struct vma *vma = cp->vmas;
        while (vma) {
            if (vma->f)
                trace("pid[%d] f->ip: %d refcnt[1]: %d", cp->pid, vma->f->ip->inum, vma->f->ip->ref);
            munmap(vma->addr, vma->length);
            vma = vma->next;
        }
        cp->vmas = NULL;
    }
    //print_mmap_list(p, "after  exit");

    // Close all open files.
    for (int fd = 0; fd < NOFILE; fd++) {
        if (cp->ofile[fd]) {
            fileclose(cp->ofile[fd]);
            cp->ofile[fd] = 0;
        }
    }

    iput(cp->cwd);
    cp->cwd = 0;

    acquire(&ptable.lock);
    trace("acq ptable");

    // Parent might be sleeping in wait().
    wakeup1(cp->parent);

    // Pass abandoned children to init.
    struct list_head *q = &cp->child;
    struct proc *p, *np;
    list_foreach_safe(p, np, q, clink) {
        assert(p->parent == cp);
        p->parent = initproc;

        list_drop(&p->clink);
        list_push_back(&initproc->child, &p->clink);
        if (p->state == ZOMBIE)
            wakeup1(initproc);
    }
    assert(list_empty(q));

    // Jump into the scheduler, never to return.
    cp->state = ZOMBIE;

    swtch(&cp->context, thiscpu()->scheduler);
    panic("zombie exit");
}

/*
 * Print a process listing to console. For debugging.
 * Runs when user types ^P on console.
 */
void procdump()
{
    static char *states[] = {
        [UNUSED]   "unused  ",
        [EMBRYO]   "embryo  ",
        [SLEEPING] "sleeping",
        [RUNNABLE] "runnable",
        [RUNNING]  "running ",
        [ZOMBIE]   "zombie  "
    };
    struct proc *p;

    // Donot acquire ptable.lock to avoid deadlock
    // acquire(&ptable.lock);
    cprintf("\n");
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED)
            continue;
        if (p->parent)
            cprintf("%d %s %s ppid: %d\n", p->pid, states[p->state], p->name,
                    p->parent->pid);
        else
            cprintf("%d %s %s\n", p->pid, states[p->state], p->name);
    }
    // release(&ptable.lock);
}
