# sdカードを動かす

- u-bootの`drivers/mmc/mmc.c, sunxi_mmc.c`を流用

```bash
[0]mmc_set_mod_clk#123: mmc 0 set mod-clk req 24000000 parent 24000000 n 1 m 1 rate 24000000
[0]mmc_set_clock#1134: clock is disabled (0Hz)
[0]sunxi_mmc_set_ios_common#190: set ios: bus_width: 0, clock: 0
[0]mmc_select_mode#146: selecting mode MMC legacy (freq : 0 MHz)
[0]sunxi_mmc_set_ios_common#190: set ios: bus_width: 1, clock: 0
[0]mmc_set_clock#1134: clock is enabled (400000Hz)
[0]sunxi_mmc_set_ios_common#190: set ios: bus_width: 1, clock: 400000
[0]mmc_set_mod_clk#123: mmc 0 set mod-clk req 400000 parent 24000000 n 4 m 15 rate 400000
[0]sunxi_mmc_send_cmd_common#332: mmc 0, cmd 0(0x80008000), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#400: mmc resp 0x00000000
[0]sunxi_mmc_send_cmd_common#332: mmc 0, cmd 8(0x80000148), arg 0x000001aa
[0]sunxi_mmc_send_cmd_common#400: mmc resp 0x000001aa
[0]sunxi_mmc_send_cmd_common#332: mmc 0, cmd 55(0x80000177), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#400: mmc resp 0x00000120
[0]sunxi_mmc_send_cmd_common#332: mmc 0, cmd 41(0x80000069), arg 0x40300000
[0]sunxi_mmc_send_cmd_common#400: mmc resp 0x40ff8000
[0]sunxi_mmc_send_cmd_common#332: mmc 0, cmd 55(0x80000177), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#400: mmc resp 0x00000120
[0]sunxi_mmc_send_cmd_common#332: mmc 0, cmd 41(0x80000069), arg 0x40300000
[0]sunxi_mmc_send_cmd_common#400: mmc resp 0xc0ff8000
[0]sunxi_mmc_send_cmd_common#332: mmc 0, cmd 2(0x800001c2), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#395: mmc resp 0x6b01687b 0x60392d2a 0x44333247 0x27504853      // ここでストール
```

- memcpy()でストールしていた。xv6-sdhostのmemcpy.S, memset.Sを使うようにした
- その他、バグを潰してMBRの読み取りに成功

```bash
[0]mmc_set_mod_clk#133: mmc 0 set mod-clk req 24000000 parent 24000000 n 1 m 1 rate 24000000
[0]sunxi_cd_gpios_init#455: sunxi_cd_gpios_init ok
[0]mmc_set_clock#1149: clock is disabled (0Hz)
[0]mmc_select_mode#158: selecting mode MMC legacy (freq : 0 MHz)
[0]mmc_set_clock#1149: clock is enabled (400000Hz)
[0]mmc_set_mod_clk#133: mmc 0 set mod-clk req 400000 parent 24000000 n 4 m 15 rate 400000
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 0(0x80008000), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000000
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 8(0x80000148), arg 0x000001aa
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x000001aa
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 55(0x80000177), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000120
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 41(0x80000069), arg 0x40300000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x40ff8000
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 55(0x80000177), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000120
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 41(0x80000069), arg 0x40300000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0xc0ff8000
[0]sd_send_op_cond#514: ocr: 0xc0ff8000, hc: 0x1
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 2(0x800001c2), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#405: mmc resp 0x6b01687b 0x60392d2a 0x44333247 0x27504853
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 3(0x80000143), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00010520
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 9(0x800001c9), arg 0x00010000
[0]sunxi_mmc_send_cmd_common#405: mmc resp 0x0a4000fb 0xe8f37f80 0x5b590000 0x400e0032
[0]mmc_select_mode#158: selecting mode MMC legacy (freq : 25 MHz)
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 7(0x80000147), arg 0x00010000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000700
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 55(0x80000177), arg 0x00010000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000920
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 51(0x80002373), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#358: trans data 8 bytes
[0]sunxi_mmc_send_cmd_common#376: cacl timeout 78 msec
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000920
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 6(0x80002346), arg 0x00fffff1
[0]sunxi_mmc_send_cmd_common#358: trans data 64 bytes
[0]sunxi_mmc_send_cmd_common#376: cacl timeout 78 msec
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000900
[0]sd_select_mode_and_width#1258: trying mode SD High Speed (50MHz) width 4 (at 50 MHz)
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 55(0x80000177), arg 0x00010000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000920
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 6(0x80000146), arg 0x00000002
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000920
[0]mmc_set_mod_clk#133: mmc 0 set mod-clk req 400000 parent 24000000 n 4 m 15 rate 400000
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 6(0x80002346), arg 0x80fffff1
[0]sunxi_mmc_send_cmd_common#358: trans data 64 bytes
[0]sunxi_mmc_send_cmd_common#376: cacl timeout 78 msec
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000900
[0]mmc_select_mode#158: selecting mode SD High Speed (50MHz) (freq : 50 MHz)
[0]mmc_set_clock#1149: clock is enabled (50000000Hz)
[0]mmc_set_mod_clk#133: mmc 0 set mod-clk req 50000000 parent 600000000 n 1 m 12 rate 50000000
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 55(0x80000177), arg 0x00010000
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000920
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 13(0x8000234d), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#358: trans data 64 bytes
[0]sunxi_mmc_send_cmd_common#376: cacl timeout 78 msec
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000920
[0]mmc_initialize#2093: ok
[0]sunxi_mmc_send_cmd_common#342: mmc 0, cmd 18(0x80003352), arg 0x00000000
[0]sunxi_mmc_send_cmd_common#358: trans data 1024 bytes
[0]sunxi_mmc_send_cmd_common#376: cacl timeout 78 msec
[0]sunxi_mmc_send_cmd_common#410: mmc resp 0x00000900
[0]sunxi_mmc_send_cmd_common#314: mmc cmd 12 check rsp busy
[0]sd_init#70: partition[0]: TYPE: 12, LBA = 0xa000, #SECS = 0x19000
[0]sd_init#70: partition[1]: TYPE: 131, LBA = 0x23000, #SECS = 0xf000
[0]sd_init#75: sd_init ok
```
