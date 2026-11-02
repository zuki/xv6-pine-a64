#include <sleeplock.h>

void
initsleeplock(struct sleeplock *lk, char *name)
{
    initlock(&lk->lk, "sleep");
    lk->locked = 0;
    lk->pid = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
    acquire(&lk->lk);
    trace("acq sleep");
    while (lk->locked) {
        sleep(lk, &lk->lk);
    }
    lk->locked = 1;
    lk->pid = thisproc()->pid;
    release(&lk->lk);
    trace("rel sleep");
}

void
releasesleep(struct sleeplock *lk)
{
    acquire(&lk->lk);
    trace("acq sleep");
    lk->locked = 0;
    lk->pid = 0;
    wakeup(lk);
    release(&lk->lk);
    trace("rel sleep");
}

int
holdingsleep(struct sleeplock *lk)
{
    int r;

    acquire(&lk->lk);
    trace("acq sleep");
    r = lk->locked && (lk->pid == thisproc()->pid);
    release(&lk->lk);
    trace("rel sleep");
    return r;
}
