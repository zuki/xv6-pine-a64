# カーネル仮想アドレスを高位アドレスに変更

カーネル仮想アドレスを`0xFFFF00000000000`以上の高位アドレスに変更

## メモ

- entry.Sのどこかでストール (2026/10/11)
- mmuを有効にするまでは正常に稼働していることを確認

```bash
Welcome to minicom 2.10

OPTIONS:
Compiled on Feb 22 2025, 10:10:35.
Port /dev/cu.usbserial-AI057C9L, 15:16:38 [F]

Press Meta-Z for help on special keys


U-Boot SPL 2023.10-rc4 (Sep 03 2026 - 17:05:45 +0900)
DRAM: 1024 MiB
Trying to boot from MMC1
NOTICE:  BL31: v2.9(debug):5f01b0b
NOTICE:  BL31: Built : 10:14:49, Sep  3 2023
NOTICE:  BL31: Detected Allwinner A64/H64/R18 SoC (1689)
NOTICE:  BL31: Found U-Boot DTB at 0x20a4c08, model: Pine64+
INFO:    ARM GICv2 driver initialized
INFO:    Configuring SPC Controller
INFO:    PMIC: Probing AXP803 on RSB
INFO:    PMIC: dcdc1 voltage: 3.300V
INFO:    PMIC: dcdc5 voltage: 1.360V
INFO:    PMIC: dcdc6 voltage: 1.100V
INFO:    PMIC: dldo1 voltage: 3.300V
INFO:    PMIC: dldo2 voltage: 3.300V
INFO:    PMIC: dldo4 voltage: 3.300V
INFO:    PMIC: fldo1 voltage: 1.200V
INFO:    PMIC: Enabling DC SW
INFO:    BL31: Platform setup done
INFO:    BL31: Initializing runtime services
INFO:    BL31: cortex_a53: CPU workaround for 843419 was applied
INFO:    BL31: cortex_a53: CPU workaround for 855873 was applied
INFO:    BL31: cortex_a53: CPU workaround for 1530924 was applied
SCP/INF: Crust v0.5.10000
INFO:    PSCI: Suspend is available via SCPI
INFO:    BL31: Preparing for EL3 exit to normal world
INFO:    Entry point address = 0x4a000000
INFO:    SPSR = 0x3c9


U-Boot 2023.10-rc4 (Sep 03 2026 - 17:05:45 +0900) Allwinner Technology

CPU:   Allwinner A64 (SUN50I)
Model: Pine64+
DRAM:  1 GiB
Core:  68 devices, 20 uclasses, devicetree: separate
WDT:   Not starting watchdog@1c20ca0
MMC:   mmc@1c0f000: 0
Loading Environment from FAT... OK
In:    serial,usbkbd
Out:   serial,vidconsole
Err:   serial,vidconsole
Net:   eth0: ethernet@1c30000
Unknown command 'nothing' - try 'help'
Hit any key to stop autoboot:  0
ethernet@1c30000 Waiting for PHY auto negotiation to complete...... done
BOOTP broadcast 1
BOOTP broadcast 2
DHCP client bound to address 192.168.10.114 (255 ms)
Using ethernet@1c30000 device
TFTP from server 192.168.10.105; our IP address is 192.168.10.114
Filename '/private/tftpboot/xv6.itb'.
Load address: 0x50000000
Loading: ########
         383.8 KiB/s
done
Bytes transferred = 107051 (1a22b hex)
## Loading kernel from FIT Image at 50000000 ...
   Using 'config-1' configuration
   Trying 'kernel' kernel subimage
     Description:  xv6 kernel
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0x500000bc
     Data Size:    65560 Bytes = 64 KiB
     Architecture: AArch64
     OS:           Linux
     Load Address: 0x42000000
     Entry Point:  0x42000000
   Verifying Hash Integrity ... OK
## Loading fdt from FIT Image at 50000000 ...
   Using 'config-1' configuration
   Trying 'fdt' fdt subimage
     Description:  pine A64+ dtb
     Type:         Flat Device Tree
     Compression:  uncompressed
     Data Start:   0x50010178
     Data Size:    39840 Bytes = 38.9 KiB
     Architecture: AArch64
   Verifying Hash Integrity ... OK
   Booting using the fdt blob at 0x50010178
Working FDT set to 50010178
   Loading Kernel Image
   Loading Device Tree to 0000000049ff3000, end 0000000049fffb9f ... OK
Working FDT set to 49ff3000

Starting kernel ...                         // ストール
```

- Google Aiが教えてくれた1GBブロックマッピングで低位アドレスの恒等マッピングと
  高位アドレスのリニアマッピングを1つのLevel 1テーブルで実装したのが問題だった。
- 問題の核心についての回答は次の通り。


>    ARM64（4KBページ）において、48ビット空間を選択した場合、仮想アドレスの翻訳ルックアップは
>    必ず 「Level 0（512GB単位）」からスタートします。各階層がカバーするサイズは以下の通りです。
>
>    - Level 0: 512GB 単位（ビット47:39）
>    - Level 1: 1GB 単位（ビット38:30）
>    - Level 2: 2MB 単位（ビット29:21）
>
>    【何が起きているか】
>
>    現在、ttbr0_el1/ ttbr1_el1 に固定アドレス（0x4000_0000）をセットしていますが、CPUは
>    このアドレスをLevel 0 テーブルとして読みに行きます。しかし、プログラムがそのテーブルに
>    書き込んでいる内容は、1GBブロック（Level 1用）の記述子（Block Descriptor） です。
>
>    ldr x1, =(0x00000000 | L1_BLOCK_VALID | ...) // ビット[1:0] = 0b01 (Block)
>
>    ARM64の仕様では、Level 0 テーブルに Block Descriptor（0b01）を配置することは禁止
>    （未定義・無効） されています。Level 0 に配置できるのは、次の階層へのポインタである
>    「Table Descriptor（0b11）」のみです。    そのため、MMUをONにした瞬間に、CPUは
>    「Level 0テーブルに不正なブロック記述子がある」と検知し、即座に Translation Fault
>    (Level 0) を発生させてハングアップしています。これが、アドレスを変更しても結果が
>    変わらなかった真相です。

- Level0とlevel1の2段階のテーブルを作ることで解決した (2026/10/12)

```bash
Starting kernel ...

[0]irq_enable#154: GICD_ISENABLER[1]: 0x1
[0]irq_enable#154: GICD_ISENABLER[0]: 0x800ffff
[0]clock_init#17: clock_init ok
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

[0]rtc_init#29: rtc_init ok
[1]secondary_main#146: Hello core 1
[2]secondary_main#146: Hello core 2
[3]secondary_main#146: Hello core 3
[2]irq_enable#154: GICD_ISENABLER[0]: 0x4000ffff
[3]irq_enable#154: GICD_ISENABLER[0]: 0x4000ffff
[2]timer_init#18: timer_init [2]: arg: 0xffff000042016fcc
[0]start_secondary_cores#78: start cores ok
[3]timer_init#18: timer_init [3]: arg: 0xffff000042017fcc
[0]irq_enable#154: GICD_ISENABLER[0]: 0x4800ffff
[1]irq_enable#154: GICD_ISENABLER[0]: 0x4000ffff
[0]timer_init#18: timer_init [0]: arg: 0xffff000042014f6c
[1]timer_init#18: timer_init [1]: arg: 0xffff000042015fcc
*********
```
