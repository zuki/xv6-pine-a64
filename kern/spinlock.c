#include <arm.h>
#include <spinlock.h>
#include <console.h>

void
initlock(struct spinlock *lk, char *name)
{
    lk->locked = 0;
    lk->name = name;
}

void
acquire(struct spinlock *lk)
{
    while (lk->locked
           || __atomic_test_and_set(&lk->locked, __ATOMIC_ACQUIRE)) ;
}

void
release(struct spinlock *lk)
{
    if (!lk->locked) {
        error("error: %s is not locked", lk->name);
        return;
    }
    __atomic_clear(&lk->locked, __ATOMIC_RELEASE);
}
