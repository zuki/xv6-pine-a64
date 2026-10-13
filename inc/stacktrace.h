#ifndef INC_STACKTRACE_H
#define INC_STACKTRACE_H

#include <sched.h>

void show_stack(struct task_struct *tsk, const char *loglvl);
void show_stack_user(void);

#endif
