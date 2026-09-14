#ifndef INC_ARM_TIMIER_H
#define INC_ARM_TIMIER_H

#include <types.h>

// EL1仮想タイマー: 計時に使用;
void arm_timer_init(void);
void arm_timer_intr(void *param);
uint64_t get_cntfrq_el0(void);

#endif
