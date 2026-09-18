#include <types.h>
#include <arm.h>
#include <string.h>
#include <console.h>
#include <kalloc.h>
#include <irq.h>
#include <timer.h>
#include <arm_timer.h>
#include <clock.h>
#include <trap.h>
#include <rtc.h>
#include <peripherals/uart.h>

void main(void)
{
    // 1文字デバッグ出力
/*
    *(volatile uint32_t *)0x01c28000 = 'M';

    // メモリバリア（C言語のインラインアセンブラ）
    asm volatile("dsb sy" : : : "memory");
    asm volatile("isb" : : : "memory");
*/
    /*
     * Before doing anything else, we need to ensure that all
     * static/global variables start out zero.
     */
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    alloc_init();
    irq_init();         // ap
    console_init();
    trap_init();        // ap
    clock_init();
    timer_init();       // ap
    enable_irq();
    //sti();

#if 0
    // 同期エラーを発生させてtrapが発生するかチェック -> voctorsのメモリ設定の修正で発生
    asm volatile("brk #0" : : : "memory");
    // timer値がカウントダウンするかチェック -> する
    delay(1000);
    debug("timer0 count: 0x%x", get32(TMR0_CUR_VALUE_REG));
    debug("timer1 count: 0x%x", get32(TMR1_CUR_VALUE_REG));
    // 割り込みが通知されるかチェック -> されない
    debug("ISPENDR[1]: 0x%x", get32(GICD_ICPENDRn + 4));
    delay(12000000);
    // timerで保留フラグが立つかチェック -> 立つ
    debug("timer pending: 0x%x", get32(TMR_IRQ_STA_REG));
    // SGI割り込みを自分自身に送り割り込みが発生するかチェック -> 発生
    irq_send_sgi_to_me(1);
    // カウンタ周波数を取得 -> 24MHz
    debug("cntfrq_el0: 0x%x", get_cntfrq_el0());
    // uartに直接書き込み、割り込みが発生するか確認　-> 発生
    put32(UART_THR(0), 0x23);
    delay(100);
    put32(UART_THR(0), 0x23);

#endif

    while (1) ;
}
