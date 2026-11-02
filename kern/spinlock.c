#include <arm.h>
#include <spinlock.h>
#include <console.h>
#include <proc.h>

void initlock(struct spinlock *lk, char *name)
{
    lk->locked = 0;
    lk->name = name;
    //lk->cpu = 0;
    __sync_synchronize();
}

void acquire(struct spinlock *lk)
{
#if 0
    push_off();
    if (!lk || holding(lk)) {
        //panic("%s is locked", lk ? lk->name : "(null)");
        panic("%p is locked", lk);
    }
#endif
    while (lk->locked
           || __atomic_test_and_set(&lk->locked, __ATOMIC_ACQUIRE)) {
        asm volatile("yield");
    }
    __sync_synchronize();
    //lk->cpu = thiscpu();
}

void release(struct spinlock *lk)
{
#if 0
    if (!lk || !holding(lk)) {
        panic("%s is not locked", lk ? lk->name : "(null)");
    }

    if (!lk->locked) {
        error("error: %s is not locked", lk->name);
        return;
    }

    lk->cpu = 0;
#endif

    if (!lk->locked) {
        //error("%s is not locked", lk->name);
        return;
    }

    __sync_synchronize();

    __atomic_clear(&lk->locked, __ATOMIC_RELEASE);

    //pop_off();
}

#if 0
int holding(struct spinlock *lk)
{
    return (lk->locked && lk->cpu == thiscpu());
}

void push_off(void)
{
    int old = intr_get();
    disable_irq();
    if (thiscpu()->noff == 0)
        thiscpu()->intena = old;
    thiscpu()->noff += 1;
}

void pop_off(void)
{
    struct cpu *cpu = thiscpu();
    if (intr_get())
        panic("pop_off: interruptible");
    if (cpu->noff < 1)
        panic("pop_off: noff");
    cpu->noff -= 1;
    if (cpu->noff == 0 && cpu->intena)
        enable_irq();
}
#endif
