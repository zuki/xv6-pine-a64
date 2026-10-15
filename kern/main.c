#include <types.h>
#include <arm.h>
#include <string.h>
#include <console.h>
#include <mm.h>
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
#include <sd.h>

int core_flags[NCPU] = { 0 };

#define PSCI_0_2_FN64_CPU_ON  0xC4000003

extern uint64_t _bss_start;
extern uint64_t _bss_end;

extern uint64_t user_begin;
extern uint64_t user_end;
extern void user_process();

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

// initで起動されるカーネルスレッドの本体
void kernel_process(void)
{
    uint64_t begin = (uint64_t)&user_begin;
    uint64_t end   = (uint64_t)&user_end;
    uint64_t process = (uint64_t)&user_process;

    int err = move_to_user_mode(begin, end - begin, process - begin);
    if (err < 0) {
        error("Error while moving process to user mode");
    } else {
        info("move_to_user_mode ok");
    }
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
        int ret = psci_cpu_on(i, V2P(&_start), 0);

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
            delay_us(1000);
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
    *(volatile uint32_t *)V2P(0xffff000001c28000) = 'M';
    // メモリバリア（C言語のインラインアセンブラ）
    asm volatile("dsb sy" : : : "memory");
    asm volatile("isb" : : : "memory");
#endif

    // bssの初期化
    uint64_t *bss = &_bss_start;
    while (bss < &_bss_end) {
        *bss++ = 0;
    }

    // カーネルページアドレスをkpgrに再セット
    set_kpgdir();
    console_init();
    info("console ok");
    mm_init();
    sched_init();
    irq_init_gicd();
    irq_init_gicc(0);
    clock_init();
    binit();
    iinit();
    fileinit();
    sd_init();
    rtc_init();
    start_secondary_cores();
    enable_irq();
    timer_init(0);       // ap
    schedule();

#if 0
    // 同期エラーを発生させてtrapが発生するかチェック -> voctorsのメモリ設定の修正で発生
    asm volatile("brk #0" : : : "memory");
    // timer値がカウントダウンするかチェック -> する
    delay(1000);
    debug("timer0 count: 0x%x", get32va(TMR0_CUR_VALUE_REG));
    debug("timer1 count: 0x%x", get32va(TMR1_CUR_VALUE_REG));
    // 割り込みが通知されるかチェック -> されない
    debug("ISPENDR[1]: 0x%x", get32va(GICD_ICPENDRn + 4));
    delay(12000000);
    // timerで保留フラグが立つかチェック -> 立つ
    debug("timer pending: 0x%x", get32va(TMR_IRQ_STA_REG));
    // SGI割り込みを自分自身に送り割り込みが発生するかチェック -> 発生
    irq_send_sgi_to_me(1);
    // カウンタ周波数を取得 -> 24MHz
    debug("cntfrq_el0: 0x%x", get_cntfrq_el0());
    // uartに直接書き込み、割り込みが発生するか確認　-> 発生
    put32va(UART_THR(0), 0x23);
    delay(100);
    put32va(UART_THR(0), 0x23);

#endif

    while (1) {
        asm volatile("wfi");
    }
}

void secondary_main(int core)
{
    core_flags[core] = 1;
    __asm_flush_dcache_line(&core_flags[core]);

    info("Hello core %d", core);

    // カーネルページアドレスをkpgrに再セット
    set_kpgdir();

    irq_init_gicc(core);
    timer_init(core);
    enable_irq();

    while (1)
        ;

}

// (アイドル以外の）最初のタスク, sched_init()で作成される
void init(int arg)
{
    int wpid;

    int res = copy_process(PF_KTHREAD, (uint64_t)&kernel_process, 0, "kern-1");
    assert(res >= 0);

    while (1) {
        wpid = wait4(0, 0, 0, 0);
        if (wpid < 0) {
            panic("init: wait failed with %d", wpid);
        } else {
            info("wait returns pid=%d", wpid);
        }
    }
}
