# 実習1

- 実行するもストール

## google aiに聞く

### デバッグ法を伝授

- 以下のコードを`entry.S`に逐次挿入して指定の文字（この場合は '3'）が出力されて
  無限ループするかをチェックする。出力されない場合、その上の行が問題箇所となる。

```bash
    ldr     x10, =UART_THR(0)
    mov     w11, #51
    strb    w11, [x10]
    dsb     sy
    isb
    b     .
```

- el2からel1に移行するところでまずストールしていることが判明
- u-bootのgoコマンドはキャッシュをクリアせずにプログラムに渡すのでそれが問題ではないか。
  u-bootのdcache, icacheコマンドでクリアしてからgoコマンドを実行せよとの指摘

### u-bootの再ビルド

- 手元のu-bootはこれらのコマンドが使えるように構成されていなかったので`CONFIG_CMD_CACHE=y`で
  再ビルド
- これでel2からel1への移行でストールしなくなる。

## 問題箇所が判明

- このMMUの有効化が問題のようだ。kpgdirの設定があやしい。

```bash
    /* Modify System Control Register to enable MMU. */
    mrs     x0, sctlr_el1
    orr     x0, x0, #SCTLR_MMU_ENABLED
    msr     sctlr_el1, x0
```

- mmu.hをxv6-sdhostのコードに入れ替え、kpgdirをpine64に合わせて修正したところ
  entry.Sからmainの呼び出しまで動くようになった。

## xv6のFIT化によるbootmコマンドの使用

- goコマンドはただ指定のアドレスに制御を渡すだけなので、bootmコマンドを使うことにして
  xv6イメージをfit化することにした。
- mkimageコマンドは既にhomebrewでインストール済みだった
- 次のようなitsファイルを作成した
- `mkimage -f xv6.its xv6.itb`でfitイメージを作成
- u-bootのコマンドプロンプトから `bootp 0x50000000 192.168.10.105:/private/tftpboot/xv6.itb ; bootm`で実行
- 結果はgoコマンドと同じで、mainの呼び出しでストール

```bash
/dts-v1/;

/ {
    description = "xv6 for Pine A64+";
    #address-cells = <1>;

    images {
        kernel {
            description = "xv6 kernel";
            data = /incbin/("xv6.bin");
            type = "kernel";
            arch = "arm64";
            os = "linux"; // U-BootにLinuxと同等に扱わせるため
            compression = "none";
            load = <0x42000000>;      // xv6を配置したいメモリのアドレス
            entry = <0x42000000>;     // xv6のエントリポイント
        };
        fdt {
            description = "pine A64+ dtb";
            data = /incbin/("sun50i-a64-pine64-plus.dtb");
            type = "flat_dt";
            arch = "arm64";
            compression = "none";
        };
    };

    configurations {
        default = "config-1";
        config-1 {
            description = "Boot xv6";
            kernel = "kernel";
            fdt = "fdt";
        };
    };
};
```

## 解決

- 原因はuart_init()のバグ（閉じカッコの位置がズレてた）だった
- `*(volatile uint32_t *)0x01c28000 = 'M';`による1行デバッグで判明
- この修正でbootm, goコマンド共に`hello, world`の出力に成功

```bash
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
Loading: #####
         364.3 KiB/s
done
Bytes transferred = 61991 (f227 hex)
## Loading kernel from FIT Image at 50000000 ...
   Using 'config-1' configuration
   Trying 'kernel' kernel subimage
     Description:  xv6 kernel
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0x500000bc
     Data Size:    20497 Bytes = 20 KiB
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
     Data Start:   0x50005174
     Data Size:    39840 Bytes = 38.9 KiB
     Architecture: AArch64
   Verifying Hash Integrity ... OK
   Booting using the fdt blob at 0x50005174
Working FDT set to 50005174
   Loading Kernel Image
   Loading Device Tree to 0000000049ff3000, end 0000000049fffb9f ... OK
Working FDT set to 49ff3000

Starting kernel ...

hello, world
```
