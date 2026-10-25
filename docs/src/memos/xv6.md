# プロセスロジックをxv6で再作成

- シングルCPUで実行
- initcodeの呼び出しまではできた

```bash
[0]irq_enable#154: GICD_ISENABLER[1]: 0x00000001
[0]buddy_init#161: free_lists: 0xffff00004201ffd0, size: 0x58
[0]buddy_init#199: buddy_init ok
[0]slab_cache_init#172: slab_cache_init ok
[0]mm_init#47: mm_init ok
[0]irq_enable#154: GICD_ISENABLER[0]: 0x0800ffff
[0]clock_init#17: clock_init ok
[0]binit#53: binit ok
[0]mmc_set_mod_clk#133: mmc 0 set mod-clk req 24000000 parent 24000000 n 1 m 1 rate 24000000
[0]sunxi_cd_gpios_init#455: sunxi_cd_gpios_init ok
[0]mmc_set_clock#1149: clock is disabled (0Hz)
[0]mmc_select_mode#158: selecting mode MMC legacy (freq : 0 MHz)
[0]mmc_set_clock#1149: clock is enabled (400000Hz)
[0]mmc_set_mod_clk#133: mmc 0 set mod-clk req 400000 parent 24000000 n 4 m 15 rate 400000
[0]sd_send_op_cond#514: ocr: 0xc0ff8000, hc: 0x1
[0]mmc_start_init#2062: mmc_start_init ok
[0]mmc_select_mode#158: selecting mode MMC legacy (freq : 25 MHz)
[0]mmc_set_mod_clk#133: mmc 0 set mod-clk req 400000 parent 24000000 n 4 m 15 rate 400000
[0]mmc_select_mode#158: selecting mode SD High Speed (50MHz) (freq : 50 MHz)
[0]mmc_set_clock#1149: clock is enabled (50000000Hz)
[0]mmc_set_mod_clk#133: mmc 0 set mod-clk req 50000000 parent 600000000 n 1 m 12 rate 50000000
[0]mmc_complete_init#2085: mmc_complete_init ok
[0]mmc_initialize#2109: mmc_initialze ok
[0]sd_init#65: partition[0]: TYPE: 12, LBA = 0xa000, #SECS = 0x19000
[0]sd_init#65: partition[1]: TYPE: 131, LBA = 0x23000, #SECS = 0xf000
[0]sd_init#70: sd_init ok

[0]rtc_init#29: rtc_init ok
[0]irq_enable#154: GICD_ISENABLER[0]: 0x4800ffff
[0]timer_init#18: timer_init [0]: arg: 0xffff00004202bfec
[0]trap_init#18: trap_init ok
[0]iinit#186: sb: size 270544960 nblocks 7000 ninodes 6989 nlog 200 logstart 1 inodestart 2 bmapstart 3
[0]syscall1#343: proc[1] sys_execve called
PANIC[0]ilock#320: ilock: no type
```
