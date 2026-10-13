#ifndef INC_TRAP_H
#define INC_TRAP_H

#ifndef __ASSEMBLER__
#include <types.h>

struct trapframe {
    uint64_t regs[31];
    uint64_t sp, pc, pstate;
    uint64_t stackfarme[2];     // 巻き戻し用
};

void trap(struct trapframe *);
void trap_init(void);

void show_invalid_entry_message(int type, uint64_t esr,
    uint64_t elr, uint64_t far);

#endif      // __ASSEMBLER__

#endif
