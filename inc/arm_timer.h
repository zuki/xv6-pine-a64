#ifndef INC_ARM_TIMIER_H
#define INC_ARM_TIMIER_H

#include <types.h>

// EL1仮想タイマー: 計時に使用, cpu0のみ;
void arm_vtimer_init(void);
void arm_vtimer_intr(void *param);
void arm_vtimer_reset(void);

// EL1物理タイマー:
void arm_ptimer_init(void);
void arm_ptimer_intr(void *param);
void arm_ptimer_reset(void);

uint64_t get_cntfrq_el0(void);

#endif
