#ifndef H_SCHED_H
#define H_SCHED_H

#ifndef __ASSEMBLER__
#include <types.h>

#define THREAD_SIZE     PGSIZE          // タスクごとのカーネルスタックサイズ

#define SYSFREQ         24000000        // 24MHz
#define HZ              100
#define INTERVAL        (SYSFREQ / HZ)  // 10ms用

extern uint64_t sys_clock;

#endif  // __ASSEMBLER__

#endif
