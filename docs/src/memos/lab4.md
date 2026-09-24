# 演習4: マルチコアとロック

## マルチコアの実装

- xv6ではなくuvaの実装を使用する
- 大きな違いは
    - BSPが立ち上がるまでAPをspinさせ、BSPからAPを起動させる
    - 各コアのidleプロセスを変更する


## 実装

- 2026/09/23: apが立ち上がらない（spin_corenの内容が変わらない）

```bash
[0]clock_init#17: clock_init ok
[0]rtc_init#29: rtc_init ok
[0]start_secondary_cores#28: core_flags[1]: 0x420001b8 = 0x4200000c
[0]start_secondary_cores#28: core_flags[2]: 0x420001c0 = 0x4200000c
[0]start_secondary_cores#28: core_flags[3]: 0x420001c8 = 0x4200000c
[0]start_secondary_cores#41: failed to start core 1
[0]start_secondary_cores#41: failed to start core 2
[0]start_secondary_cores#41: failed to start core 3
[0]start_secondary_cores#49: start cores ng
[0]timer_init#15: timer_init ok
**$**$**$*
```

- 2026/09/24: cpu1-3はそもそも起動していなかった
    - spinに分岐させる前にコアIDを出力させたところ'0'しか出力されないことで発見
- そのため、spin park方式はダメで、PSCI SMCコール（ATFの機能）で各コアを起動させる必要があった。
    - google ai（「Pine A64への移植: SMPコア起動」スレッド参照）

```c
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
```


```bash
Starting kernel ...

[0]clock_init#17: clock_init ok
[0]rtc_init#29: rtc_init ok
[1]secondary_main#140: Hello core 1
[2]secondary_main#140: Hello core 2
[3]secondary_main#140: Hello core 3
[0]start_secondary_cores#81: start cores ok
[0]timer_init#15: timer_init ok
**$*
```
