#ifndef INC_SPINLOCK_H
#define INC_SPINLOCK_H

struct spinlock {
    volatile unsigned char locked;
    //struct cpu *cpu;
    char *name;
};

void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int  holding(struct spinlock *lk);
void push_off(void);
void pop_off(void);

#endif
