#ifndef INC_SLEEPLOCK_H
#define INC_SLEEPLOCK_H

#include <spinlock.h>

// Long-term locks for processes
struct sleeplock {
  unsigned int locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock

  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

#endif
