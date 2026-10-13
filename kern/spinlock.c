#include <arm.h>
#include <spinlock.h>
#include <sched.h>
#include <console.h>

void initlock(struct spinlock *lk, char *name)
{
    lk->locked = 0;
    lk->name = name;
    lk->cpu = 0;
}

void
acquire(struct spinlock *lk)
{
    push_off();
    if (!lk || holding(lk)) {
        panic("%s is locked", lk->name);
    }
    while (lk->locked
           || __atomic_test_and_set(&lk->locked, __ATOMIC_ACQUIRE)) ;

    __sync_synchronize();
    lk->cpu = mycpu();
}

void release(struct spinlock *lk)
{
    if (!lk || !holding(lk)) {
        panic("%s is not locked", lk->name);
    }
    __sync_synchronize();

    __atomic_clear(&lk->locked, __ATOMIC_RELEASE);

    pop_off();
}

int holding(struct spinlock *lk)
{
    return (lk->locked && lk->cpu == mycpu());
}

void push_off(void)
{
    int old = intr_get();

    // intr_off();
    disable_irq();
    if (mycpu()->noff == 0)
            mycpu()->intena = old;
    mycpu()->noff += 1;
}

void pop_off(void)
{
    struct cpu *c = mycpu();
    if (intr_get())
        panic("pop_off - interruptible");
    if (c->noff < 1)
        panic("pop_off");
    c->noff -= 1;
    if (c->noff == 0 && c->intena)
        enable_irq();
}
