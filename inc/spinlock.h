#ifndef INC_SPINLOCK_H
#define INC_SPINLOCK_H

struct cpu;

struct spinlock {
    volatile int locked;
    struct cpu *cpu;
    char *name;
};

#if 0
/* セマフォテーブルエントリ */
struct sem {
  char used;    // 0 if free; otherwise used
  int count;    // sem count
};
#endif

void initlock(struct spinlock *, char *name);
void acquire(struct spinlock *);
void release(struct spinlock *);
int  holding(struct spinlock *lk);
void push_off(void);
void pop_off(void);

#endif
