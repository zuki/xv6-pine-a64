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
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <rtc.h>
#include <peripherals/uart.h>
#include <utils.h>

int core_flags[NCPU] = { 0 };

#define PSCI_0_2_FN64_CPU_ON  0xC4000003

// PSCIを呼び出すアセンブリラッパー
static int psci_cpu_on(unsigned long cpuid, unsigned long entry_point, unsigned long context) {
    register unsigned long r0 asm("x0") = PSCI_0_2_FN64_CPU_ON;
    register unsigned long r1 asm("x1") = cpuid;
    register unsigned long r2 asm("x2") = entry_point;
    register unsigned long r3 asm("x3") = context;

    asm volatile(
        "smc #0\n"
        : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r3)
        : "memory"
    );
    return r0; // 0が返ってくれば成功
}


static void start_secondary_cores(void)
{
    int ng = 0;
    for (int i = 1; i < NCPU; i++) {
        core_flags[i] = 0;
        __asm_flush_dcache_line(&core_flags[i]);

        // ATF(PSCI)に対して、「コアi を 物理アドレス _start から起動せよ」と命令する
        // ※ entry_point にはAPが最初に実行すべき物理アドレス（_start）を指定します
        extern void _start(void);
        int ret = psci_cpu_on(i, (unsigned long)&_start, 0);

        if (ret != 0) {
            error("PSCI CPU_ON failed for core %d, ret=%d", i, ret);
            ng++;
            continue;
        }

        int timeout = 100;
        while (1) {
            __asm_invalidate_dcache_line(&core_flags[i]);

            if (core_flags[i] == 1) break;

            if (timeout-- <= 0) {
                error("failed to start core %d", i);
                ng++;
                break;
            }
            delayus(1000);
        }
    }
    if (ng) {
        info("start cores ng");
    } else {
        info("start cores ok");
    }
}

void main(void)
{
#if 0
    // 1文字デバッグ出力
    *(volatile uint32_t *)0x01c28000 = 'M';
    // メモリバリア（C言語のインラインアセンブラ）
    asm volatile("dsb sy" : : : "memory");
    asm volatile("isb" : : : "memory");
#endif

    alloc_init();
    irq_init_gicd();
    irq_init_gicc(0);
    console_init();
    clock_init();

    rtc_init();
    start_secondary_cores();
    enable_irq();
    timer_init(0);       // ap

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

void secondary_main(int core)
{
    core_flags[core] = 1;
    __asm_flush_dcache_line(&core_flags[core]);

    info("Hello core %d", core);

    irq_init_gicc(core);
    timer_init(core);
    enable_irq();

    while (1)
        ;

}
