#include <param.h>
#include <mmu.h>
#include <sched.h>
#include <arm_timer.h>

// ２種類のタイマーで計時とスケジューラを駆動する（どちらも1tick=10ms）
//   - コアごとにあるARM汎用物理タイマー: スケジューラtickとして使用
//   - コア0のAARM汎用仮想タイマー: 計時tickとして使用

uint32_t ticks;

// ブート時に使用され、起動後はアイドルタスク用のカーネルスタックとして使用される
__attribute__ ((aligned (PGSIZE))) char boot_stacks[NCPU][THREAD_SIZE];
