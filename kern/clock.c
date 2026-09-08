#include <clock.h>

#include <arm.h>
#include <irq.h>
#include <timer.h>
#include <console.h>

// Timer 0をクロックタイマーとして使用する

void clock_init(void)
{
    // Timer0は10ms毎に割り込んでtickを進める
    put32(TMR0_INTV_VALUE_REG, 1200);
    clock_reset();
    put32(TMR0_CTRL_REG, TMR_MODE_SINGLE | TMR_CLK_PRES(1) | TMR_CLK_SRC_24M);
    put32(TMR0_CTRL_REG, get32(TMR0_CTRL_REG) | TMR_RELOAD);
    while(get32(TMR0_CTRL_REG) & TMR_RELOAD);
    put32(TMR0_CTRL_REG, get32(TMR0_CTRL_REG) | TMR_EN);

    irq_enable(IRQ_TIMER0);
    irq_register(IRQ_TIMER0, clock_intr, 0);
}

void clock_reset(void)
{
    put32(TMR0_CTRL_REG, get32(TMR0_CTRL_REG) | TMR_RELOAD);
}

/*
 * TODO: tickを進める
 */
void clock_intr(void *param)
{
    cprintf("cpu %d clock.\n", cpuid());
}
