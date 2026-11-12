#ifndef INC_CLOCK_H
#define INC_CLOCK_H

void clock_init(void);
void clock_intr(void *param);
void arm_vtimer_init(void);
void arm_vtimer_reset(void);
int clock_gettime(clockid_t clk_id, uint64_t tpp, int user);
int clock_settime(clockid_t clk_id, uint64_t tpp);

#endif
