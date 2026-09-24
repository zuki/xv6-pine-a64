#include <param.h>
#include <mmu.h>
#include <sched.h>

// ブート時に使用され、起動後はアイドルタスク用のカーネルスタックとして使用される
__attribute__ ((aligned (PGSIZE))) char boot_stacks[NCPU][THREAD_SIZE];
