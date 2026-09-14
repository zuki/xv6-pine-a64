#ifndef INC_TIMER_H
#define INC_TIMER_H

#include <peripherals/base.h>

#define TIMER_BASE          (MMIO_BASE + 0x01C20C00)
#define TMR_IRQ_EN_REG	    (TIMER_BASE + 0x0)
#define     TMR1_IRQ_EN         (1 << 1)
#define     TMR0_IRQ_EN         (1 << 0)
#define TMR_IRQ_STA_REG	    (TIMER_BASE + 0x4)
#define     TMR1_IRQ_PEND       (1 << 1)
#define     TMR0_IRQ_PEND       (1 << 0)
#define TMR0_CTRL_REG	    (TIMER_BASE + 0x10)
#define TMR0_INTV_VALUE_REG	(TIMER_BASE + 0x14)
#define TMR0_CUR_VALUE_REG	(TIMER_BASE + 0x18)
#define TMR1_CTRL_REG	    (TIMER_BASE + 0x20)
#define TMR1_INTV_VALUE_REG	(TIMER_BASE + 0x24)
#define TMR1_CUR_VALUE_REG	(TIMER_BASE + 0x28)

#define TMR_MODE_CONT       (0 << 7)
#define TMR_MODE_SINGLE     (1 << 7)
#define TMR_CLK_PRES(n)     ((n) << 4)  /* 0<=n<=7 */
#define TMR_CLK_SRC_32K     (0 << 2)
#define TMR_CLK_SRC_24M     (1 << 2)
#define TMR_RELOAD          (1 << 1)
#define TMR_EN              (1 << 0)

// ハイスピードタイマー
#define HSTIMER_BASE        (MMIO_BASE + 0x01c60000)

#define HS_TMR_IRQ_EN_REG   (HSTIMER_BASE + 0x00)
#define HS_TMR_STAS_REG     (HSTIMER_BASE + 0x04)
#define HS_TMR_CTRL_REG     (HSTIMER_BASE + 0x10)
#define     HS_TMR_CTRL_NORMAL  (0 << 31)
#define     HS_TMR_CTRL_MODE_CONT (0 << 7)
#define     HS_TMR_CTRL_CLK(n)   ((n) << 4)
#define     HS_TMR_CTRL_RELOAD  (1 << 1)
#define     HS_TMR_CTRL_EN      (1 << 0)
#define HS_TMR_INTV_LO_REG  (HSTIMER_BASE + 0x14)
#define HS_TMR_INTV_HI_REG  (HSTIMER_BASE + 0x18)
#define HS_TMR_CUR_LO_REG   (HSTIMER_BASE + 0x1c)
#define HS_TMR_CUR_HI_REG   (HSTIMER_BASE + 0x20)


void timer_init(void);
void timer_intr(void *param);
void timer_reset(void);

void arm_timer_init(void);
void arm_timer_intr(void *param);
void arm_timer_reset(void);

#endif
