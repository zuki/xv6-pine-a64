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

## 2026/10/26

- file.c, fs.c, mkfs.cなどを修正

```bash
[0]rtc_init#29: rtc_init ok
[0]irq_enable#154: GICD_ISENABLER[0]: 0x4800ffff
[0]timer_init#18: timer_init [0]: arg: 0xffff00004202bfec
[0]trap_init#18: trap_init ok
[0]iinit#187: sb: size 7000 nblocks 6989 ninodes 200 nlog 1 logstart 2 inodestart 3 bmapstart 10
[0]syscall1#343: proc[1] sys_execve called
[0]execve#34: path='/bin/init', argv=0x0x0, envp=0x0x0
[0]trap#48: ret: 0x0
[0]trap#87: unknown trap code: 7 at elr: 0x4007a0 with far: 0x0
PANIC[0]trap#90: unknown
```

- 0x4007a0はmemset()の1行目
- v0を使っている

```bash
00000000004007a0 <memset>:
  4007a0:       4e010c20        dup     v0.16b, w1
```

- trapframeとcontextにq0を追加して、保存・復元するようにした

```bash
[0]irq_enable#154: GICD_ISENABLER[1]: 0x00000001
[0]buddy_init#161: free_lists: 0xffff00004201ffd0, size: 0x58
[0]buddy_init#199: buddy_init ok
[0]slab_cache_init#172: slab_cache_init ok
...
[0]sd_init#70: sd_init ok

[0]rtc_init#29: rtc_init ok
[0]irq_enable#154: GICD_ISENABLER[0]: 0x4800ffff
[0]timer_init#18: timer_init [0]: arg: 0xffff00004202bfec
[0]trap_init#18: trap_init ok
                                // ここでストール
```

- memset.Sを使うように変更(buddy_initでストールしたため、memset.Sを使わないようにしていた)

```bash
[0]irq_enable#154: GICD_ISENABLER[1]: 0x00000001
[0]buddy_init#161: free_lists: 0xffff00004201ffd0, size: 0x58
                                // ここでストール : budy_initのストールが再現
```

- sindが有効になっていなかった (entry.S)
- sys_writev, sys_readvを修正
- sysfileでcopyin, copyoutが必要な場合は使用するようにした。
- /dev/consoleはmkfsで作成

```bash
[0]irq_enable#154: GICD_ISENABLER[1]: 0x00000001
[0]buddy_init#161: free_lists: 0xffff000042021fd0, size: 0x58
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
[0]timer_init#18: timer_init [0]: arg: 0xffff000042032fec
[0]trap_init#18: trap_init ok
[0]fsinit#54: sb: size 7000 nblocks 6989 ninodes 200 nlog 1 logstart 2 inodestart 3 bmapstart 10
[0]syscall1#343: proc[1] sys_execve called
[0]execve#34: path='/bin/init', argv=0x0x0, envp=0x0x0          // /bin/initの呼び出しに成功
[0]syscall1#343: proc[1] sys_gettid called
[0]syscall1#343: proc[1] sys_openat called
[0]sys_openat#385: dirfd -100, path '/dev/console', flag 0x20002
[0]sys_openat#398: ip->inu: 6, type: 2
[0]syscall1#343: proc[1] sys_dup called
[0]syscall1#343: proc[1] sys_dup called
[0]syscall1#343: proc[1] sys_rt_sigprocmask called
[0]syscall1#343: proc[1] sys_rt_sigprocmask called
[0]syscall1#343: proc[1] sys_clone called
[0]syscall1#343: proc[1] sys_rt_sigprocmask called
[0]syscall1#343: proc[1] sys_rt_sigprocmask called
[0]syscall1#343: proc[1] sys_wait4 called
[0]syscall1#343: proc[3] sys_gettid called
[0]syscall1#343: proc[3] sys_rt_sigprocmask called
[0]syscall1#343: proc[3] sys_rt_sigprocmask called
[0]syscall1#343: proc[3] sys_execve called
[0]execve#34: path='/bin/sh', argv=0x0x403020, envp=0x0x403000      // /bin/sh　の呼び出しに成功
[0]syscall1#343: proc[3] sys_gettid called
[0]syscall1#343: proc[3] sys_openat called
[0]sys_openat#385: dirfd -100, path '/dev/console', flag 0x20002
[0]sys_openat#398: ip->inu: 6, type: 3
[0]syscall1#343: proc[3] sys_close called
[0]syscall1#343: proc[3] sys_writev called
[0]sys_writev#139: fd 2, iovp: 0xfffffeb0, iovcnt: 2
[0]filewrite#123: f: ino: 6, writeable: 1, addr: 0x40a9dc, size: 0
[0]filewrite#123: f: ino: 6, writeable: 1, addr: 0x4087c0, size: 2  // シェルプロンプトの出力が出ない
[0]syscall1#343: proc[3] sys_read called                            // シェルからの呼び出しが止まらない
[0]syscall1#343: proc[3] sys_writev called                          // 以降、ここから
[0]sys_writev#139: fd 2, iovp: 0xfffffeb0, iovcnt: 2
[0]filewrite#123: f: ino: 6, writeable: 1, addr: 0x40a9dc, size: 0
[0]filewrite#123: f: ino: 6, writeable: 1, addr: 0x4087c0, size: 2
[0]syscall1#343: proc[3] sys_read called                            // ここまでが無限ループ
```

- consoleread(), consolewrite()が実装されていなかった

```bash
[0]execve#34: path='/bin/sh', argv=0x0x403020, envp=0x0x403000
[0]syscall1#343: proc[6] sys_gettid called
[0]syscall1#343: proc[6] sys_openat called
[0]sys_openat#391: dirfd -100, path '/dev/console', flag 0x20002
[0]sys_openat#419: ip: ino: 6, type: 3, devno: 0x100
[0]syscall1#343: proc[6] sys_close called
[0]syscall1#343: proc[6] sys_writev called
[0]sys_writev#145: fd 2, iovp: 0xfffffffffeb0, iovcnt: 2
[0]filewrite#128: f: ino: 6, writeable: 1, type: 3, addrp: 0x40a9dc, size: 0
[0]filewrite#128: f: ino: 6, writeable: 1, type: 3, addrp: 0x4087c0, size: 2
$ [0]syscall1#343: proc[6] sys_read called
[0]sys_read#84: ip: 6, bufp: 0x40a9e4, n: 1024
[0]fileread#98: f type: 3, readable: 1, major: 1, addrp: 0x40a9e4, n: 1024
                                // 入力を受け付けない
```

- コンソールから読み込みができない
    - consoleread()の最初のacquire(&g_channel.lock)でストールするため
- spinlock()にcpuを追加しても、削除しても結果は同じ
    - sys_read: fd=0 -> sysin, inum=6 -> /dev/console で問題ない
- cpuを1つにしても結果は同じ

```bash
[2]execve#34: path='/bin/sh', argv=0x0x403020, envp=0x0x403000
[2]syscall1#345: proc[6] sys_gettid called
[2]syscall1#345: proc[6] sys_openat called
[2]sys_openat#392: dirfd -100, path '/dev/console', flag 0x20002
[2]sys_openat#420: ip: ino: 6, type: 3, devno: 0x100
[2]sys_openat#436: path: /dev/console, fd: 3
[2]syscall1#345: proc[6] sys_close called
[2]syscall1#345: proc[6] sys_writev called
[2]sys_writev#146: fd 2, inum: 6, iovp: 0xfffffffffeb0, iovcnt: 2
$
[2]syscall1#345: proc[6] sys_read called
[2]sys_read#85: fd: 0, inum: 6, bufp: 0x40a9e4, n: 1024
```

- acquire()の前で、割り込み禁止、ロック取得済みとなっている

```bash
[0]consoleread#102: intena: 0, lock: 1
```

- lock=1になるのは、debug()の引数でlock.lockedを見ていたため（debug内でlockする）
  で、debug()前に見ると0であった。
- ここでgoogleに聞く
- syscall1()呼び出し前後でenable_irq(), disable_irq()が必要とのこと。確かに、
  uvaosではしていたが、rpi-osではしていない。とにかく、することで割り込みが有効になった
- しかし、結果は同じで相変わらずacquire()できず
- また、uartの割り込みが発火しない。gicの設定の問題だというが、まだ解決せず。27, 30は
  発火する（fsinit()の途中でストールした）のでspiの設定の問題と思われる。

## uartの割り込みが発火しない件

- spiの割り込みが一切発火していない模様

## GICについて

- pine a64+はGICv2を使用している
- GICv2の割り込みはセキュアなグループ0と非セキュアなグループ1に分けられてる
- この振り分けはEL3でした処理できず、現在の環境ではAT/F (Arm Trusted Firmware)の
  BL31が担っている
- AT/FはすべてのSPIをグループ1とし、グループ1は無効にしてU-Bootに渡していることが
  確認された
- U-Bootでは割り込みに対しては何もさわっていない
- GICのレジスタはアクセスに制約があるものがあり、使用環境は「GICセキュリティ
  拡張を含む実装の非セキュアなアクセス」となる。
- GICD_CTLRは使用する環境により定義が異なり、使用環境では図4-1が適用される
- GICD_IGROUPは使用環境ではRAZ/WIとなり、設定を見ることはできない(AT/Fでしか
  確認できない)
- GICC_CTLRも使用する環境により定義が異なり、使用環境では図4-23が適用される

```bash
U-Boot SPL 2023.10-rc4 (Nov 01 2026 - 11:17:12 +0900)
DRAM: 1024 MiB
Trying to boot from MMC1
NOTICE:  BL31: v2.9(debug):
NOTICE:  BL31: Built : 11:13:54, Nov  1 2026
NOTICE:  BL31: Detected Allwinner A64/H64/R18 SoC (1689)
NOTICE:  BL31: Found U-Boot DTB at 0x20a44d0, model: Pine64+
INFO:    ARM GICv2 driver initialized           // このGICD_CTLRは図4-2
INFO:    GICD_CTLR: 0x01                        // Grp0は有効、Grp1は無効
INFO:    GICD_IGROUPR 0: 0x0, 1: 0xffffffff     // 32番以降のspiはGrp1
INFO:    Configuring SPC Controller
```

- GICD, GICCの初期化の見直しでshで入力文字が表示されるようになった。ただし、実行はされない
- **TODO**: スプリアスIDが通知される件は調査が必要
- 外部ペリフェラルとしてのタイマーとハイスピードタイマーの割り込みも確認できた
- これらの割り込みが届かなかったのはGICC_PMRが0xFFとされ、すべての優先度を通すと考えて、
  GICD_IPRIORITYRnを0xF0としていたが、これはこれ以上ではなく、これを超える優先度を
  通すという意味だったようだ。GICD_IPRIORITYRnを0xE0とより強い優先度としたところ
  割り込みが発生するようになった。

```bash
                // 以下はGICDの初期化前の各レジスタの値
[0]irq_init_gicd#50: GICD_CTLR: 0x00000000
[0]irq_init_gicd#51: GICD_ISENABLER  0: 0x0000ffff, 1: 0x00000000
[0]irq_init_gicd#52: GICD_ICENABLER  0: 0x0000ffff, 1: 0x00000000
[0]irq_init_gicd#53: GICD_ISACTIVER  0: 0x00000000, 1: 0x00000000
[0]irq_init_gicd#54: GICD_ICACTIVER  0: 0x00000000, 1: 0x00000000
[0]irq_init_gicd#55: GICD_IPRIORITYR 0: 0x00000000, 8: 0x00000000
[0]irq_init_gicd#56: GICD_ITARGETSR  0: 0x01010101, 8: 0x00000000
[0]irq_init_gicd#57: GICD_ICFGR      0: 0xaaaaaaaa, 2: 0x55555555
                // 設定後
[0]irq_init_gicd#105: GICD_IPRIORITYRn 0: 0x80808080, 8: 0xa0a0a0a0
[0]irq_init_gicd#106: GICD_ITARGETSR   0: 0x01010101, 8: 0x0f0f0f0f

[0]irq_init_gicd#111: cpu0_init ok

                // 以下はGICCの初期化前の各レジスタの値
[0]irq_init_gicc#120: GICC_CTLR: 0x00000060
[0]irq_init_gicc#121: GICC_PMR: 0x000000f0

[0]irq_enable#147: enable: 32, GICD_ISENABLER[1]: 0x00000001
[0]irq_enable#147: enable: 27, GICD_ISENABLER[0]: 0x0800ffff
[0]irq_enable#147: enable: 30, GICD_ISENABLER[0]: 0x4800ffff

[0]main#131: GICC_CTLR: 0x61

[1]irq_init_gicc#120: GICC_CTLR: 0x00000060
[1]irq_init_gicc#121: GICC_PMR: 0x000000f0
[1]irq_enable#147: enable: 30, GICD_ISENABLER[0]: 0x4000ffff

[1]sys_writev#146: fd 2, inum: 6, iovp: 0xfffffffffeb0, iovcnt: 2
$ [1]sys_read#85: fd: 0, inum: 6, bufp: 0x40a9e4, n: 1024
[2]handle1#173: no handler for irq 1023         // スプリアスID
[3]handle1#173: no handler for irq 1023
/bin/ls                                         // 入力文字がコールバックされる
```

## /bin/lsが実行されない件

- consoleread()とconsole_intr1()でlockの競合が発生していた
- console_intr1()内でのlockの取得/解放を削除したところ、/bin/shが実行された（エラーあり）
- **TODO**: xv6-sdhostではlockがあっても競合が発生しないのはなぜか、調査が必要

```bash
$ [1]sys_read#87: fd: 0, inum: 6, bufp: 0x40a9e4, n: 1024
32 /bin/ls
[3]consoleread#126: return 8
[3]sys_read#91: read: /bin/ls
, 8 bytes
PANIC[0]sys_brk#22: sys_brk: unimplemented.
```

## 割り込みの件はひとまず解決

- sys_brk()を実装

```bash
$ /bin/ls /
[1]consoleread#126: return 10
[0]execve#34: path='/bin/ls', argv=0x0x40b008, envp=0x0x40a6b0
[0]sys_fstatat#208: flags unimplemented
can't get stat of /
[0]exit#397: exit: pid 8, err 1
```

- sys_fstatat()はflagsがあると-1を返していたが、AT_SYMLINK_NOFOLLOWフラグが指定されていた。
- 当面、warn()だけで処理を進めるように変更した

```bash
$ /bin/ls /
[3]consoleread#126: return 10
[1]execve#34: path='/bin/ls', argv=0x0x40b008, envp=0x0x40a6b0
[1]sys_fstatat#210: flags: 0x100 unimplemented
[1]sys_openat#401: dirfd -100, path '/', flag 0xa4000
[1]trap#80: triger ec: 36 at elr: 0x40089c with far: 0xc    // readdir
[1]exit#397: exit: pid 7, err 1
```

- ls/main.cを/xv6-raspi/mac-rpi-osのコードにする
- 中途半端な出力
- /bin/catは正常

```bash
$ /bin/ls /
[1]consoleread#126: return 10
[2]execve#34: path='/bin/ls', argv=0x0x40b008, envp=0x0x40a6b0
[2]sys_openat#401: dirfd -100, path '/', flag 0x20000
[2]sys_openat#429: ip: ino: 1, type: 1, devno: 0x1
[2]sys_openat#445: path: /, fd: 3
.              41fd 1 4096
               41fd 1 4096
               41fd 1 4096
lib            41ff 5 128
$ /bin/ls /bin
[2]consoleread#126: return 13
[1]execve#34: path='/bin/ls', argv=0x0x40b008, envp=0x0x40a6b0
[1]sys_openat#401: dirfd -100, path '/bin', flag 0x20000
[1]sys_openat#429: ip: ino: 2, type: 1, devno: 0x1
[1]sys_openat#445: path: /bin, fd: 3
.              41fd 2 448
               41fd 2 448
               41fd 2 448
sh             81ed 11 57448
$ /bin/ls /lib
.              41ff 5 128
$ /bin/cat test.txt
test 123
abcdef
2025-02-20
123 test
abcdef
$
```

- 再び、新しいls/main.cに戻す
- `dir = opendir(path)`で返されるdirがnull。
- muslのcalloc()でmmap()が呼び出されるが、mmap()は未実装
- 実装するしかない

```bash
$ [3]syscall1#336: proc[6] sys_read called
/bin/ls /
[1]syscall1#336: proc[6] sys_clone called
[2]syscall1#336: proc[7] sys_gettid called
[1]syscall1#336: proc[6] sys_wait4 called
[2]syscall1#336: proc[7] sys_brk called
[2]syscall1#336: proc[7] sys_brk called
[2]syscall1#336: proc[7] sys_execve called
[2]syscall1#336: proc[7] sys_gettid called
[2]syscall1#336: proc[7] sys_fstatat called
[2]sys_fstatat#214: flags: 0x100 unimplemented
[2]sys_fstatat#221: path: /, ip: 1, dev: 1
[2]syscall1#336: proc[7] sys_openat called
[2]sys_openat#407: dirfd -100, path '/', flag 0xa4000
[2]sys_openat#420: ip->inum: 1, type: 1
[2]sys_openat#436: ip: ino: 1, type: 1, devno: 0x1
[2]sys_openat#452: path: /, fd: 3
[2]syscall1#336: proc[7] sys_fcntl called
[2]sys_fcntl#625: fd: 3, cmd: 0x2
[2]syscall1#336: proc[7] sys_brk called
[2]syscall1#336: proc[7] sys_brk called
[2]syscall1#336: proc[7] sys_mmap called
[2]sys_mmap#54: addrp: 0x40f000, len: 0x1000, proc: 0x0, flags: 0x32, fd: -1, off: 0x0
[2]syscall1#336: proc[7] sys_mmap called
[2]sys_mmap#54: addrp: 0x0, len: 0x2000, proc: 0x3, flags: 0x22, fd: -1, off: 0x0
[2]syscall1#336: proc[7] sys_close called
[2]syscall1#336: proc[7] sys_ioctl called
[2]sys_ioctl#596: fd: 1, inum: 6, req: 0x5413, argp: 0xfffffffffa48
[2]syscall1#336: proc[7] sys_writev called
path: /, dir: 0x0                               // dir = opendir(path);　のdirがNULL
[2]trap#80: triger ec: 36 at elr: 0x4008b8 with far: 0xc
[2]exit#397: exit: pid 7, err 1
[1]syscall1#336: proc[6] sys_writev called
$ [1]syscall1#336: proc[6] sys_read called
```

- `aarch64-none-elf-objdump -d obj/usr/bin/ls`

```bash
00000000004008b4 <readdir>:
  4008b4:       aa0003e3        mov     x3, x0
  4008b8:       29418002        ldp     w2, w0, [x0, #12]  // x0 = 0 なので、far: [x0, #12] = 0xc
```

- mmap()ほかを実装

```bash
$ [3]syscall1#336: proc[6] sys_read called
/bin/ls /
[3]syscall1#336: proc[6] sys_clone called
[1]syscall1#336: proc[7] sys_gettid called
[3]syscall1#336: proc[6] sys_wait4 called
[1]syscall1#336: proc[7] sys_brk called
[1]syscall1#336: proc[7] sys_brk called
[1]syscall1#336: proc[7] sys_execve called
[1]syscall1#336: proc[7] sys_gettid called
[1]syscall1#336: proc[7] sys_fstatat called
[1]sys_fstatat#214: flags: 0x100 unimplemented
[1]sys_fstatat#221: path: /, ip: 1, dev: 1
[1]syscall1#336: proc[7] sys_openat called
[1]syscall1#336: proc[7] sys_fcntl called
[1]syscall1#336: proc[7] sys_brk called
[1]syscall1#336: proc[7] sys_brk called
[1]syscall1#336: proc[7] sys_mmap called
[1]sys_mmap#57: addrp: 0x40f000, length: 0x1000, proc: 0x0, flags: 0x32, fd: -1, offset: 0x0
[1]syscall1#336: proc[7] sys_mmap called
[1]sys_mmap#57: addrp: 0x0, length: 0x2000, proc: 0x3, flags: 0x22, fd: -1, offset: 0x0
[1]mmap#740: return addr: 0xffdfffff0000, length: 0x2000, prot: 0x3, flags: 0x22, f: 0, offset: 0x0
[1]syscall1#336: proc[7] sys_ioctl called
[1]syscall1#336: proc[7] sys_writev called
path: /, dir: 0xffdfffff0020                            // DIR *dirが設定されている
[1]syscall1#336: proc[7] sys_getdents64 called
[1]filegetdents#232: read invalid n=-1, tlen=0          // ここでfilereadエラー
[1]syscall1#336: proc[7] sys_close called
[1]syscall1#336: proc[7] sys_munmap called
[1]syscall1#336: proc[7] sys_exit called
[3]syscall1#336: proc[6] sys_writev called
$ [3]syscall1#336: proc[6] sys_read called
```

- fileread(), filewrite()にもtargetがuserかkernelかの区別をするフィールドが必要だった。
- 完成　(struct procにまだuid/gidと時刻がフィールドがないため表示はおかしいが)

```bash
U-Boot SPL 2023.10-rc4 (Nov 01 2026 - 11:17:12 +0900)
DRAM: 1024 MiB
Trying to boot from MMC1
NOTICE:  BL31: v2.9(debug):
NOTICE:  BL31: Built : 11:13:54, Nov  1 2026
NOTICE:  BL31: Detected Allwinner A64/H64/R18 SoC (1689)
NOTICE:  BL31: Found U-Boot DTB at 0x20a44d0, model: Pine64+
INFO:    ARM GICv2 driver initialized
INFO:    GICD_CTLR: 0x1
INFO:    GICD_IGROUPR 0: 0x0, 1: 0xffffffff
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


U-Boot 2023.10-rc4 (Nov 01 2026 - 11:17:12 +0900) Allwinner Technology

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
Loading: #############
         493.2 KiB/s
done
Bytes transferred = 189019 (2e25b hex)
## Loading kernel from FIT Image at 50000000 ...
   Using 'config-1' configuration
   Trying 'kernel' kernel subimage
     Description:  xv6 kernel
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0x500000bc
     Data Size:    147528 Bytes = 144.1 KiB
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
     Data Start:   0x500241a8
     Data Size:    39840 Bytes = 38.9 KiB
     Architecture: AArch64
   Verifying Hash Integrity ... OK
   Booting using the fdt blob at 0x500241a8
Working FDT set to 500241a8
   Loading Kernel Image
   Loading Device Tree to 0000000049ff3000, end 0000000049fffb9f ... OK
Working FDT set to 49ff3000

Starting kernel ...

[0]irq_init_gicd#111: cpu0_init ok
[0]console_init#149: console_init ok
[0]buddy_init#161: free_lists: 0xffff000042025fd0, size: 0x58
[0]buddy_init#199: buddy_init ok
[0]slab_cache_init#172: slab_cache_init ok
[0]mm_init#47: mm_init ok
[0]cachepage_init#40: cachepage_init ok
[0]clock_init#18: clock_init ok
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
[0]sd_init#66: partition[0]: TYPE: 12, LBA = 0xa000, #SECS = 0x19000
[0]sd_init#66: partition[1]: TYPE: 131, LBA = 0x23000, #SECS = 0xf000
[0]sd_init#71: sd_init ok

[0]rtc_init#29: rtc_init ok
[1]secondary_main#145: Hello core 1
[3]secondary_main#145: Hello core 3
[2]secondary_main#145: Hello core 2
[3]timer_init#18: timer_init [3]: arg: 0xffff0000421b9fcc
[2]timer_init#18: timer_init [2]: arg: 0xffff0000421b8fcc
[3]trap_init#23: trap_init ok
[1]timer_init#18: timer_init [1]: arg: 0xffff0000421b7fcc
[0]start_secondary_cores#82: start cores ok
[3]fsinit#54: sb: size 7000 nblocks 6989 ninodes 200 nlog 1 logstart 2 inodestart 3 bmapstart 10
[2]trap_init#23: trap_init ok
[1]trap_init#23: trap_init ok
[0]timer_init#18: timer_init [0]: arg: 0xffff0000421b6f5c
[0]trap_init#23: trap_init ok
$ /bin/ls /
drwxrwxr-x    1 anon anon  -8917582-02-27 01:15:00   4096 .
drwxrwxr-x    1 anon anon  -8917582-02-27 01:15:00   4096 ..
drwxrwxr-x    2 anon anon  -8917575-11-08 02:47:44    448 bin
drwxrwxr-x    3 anon anon  -8917575-11-08 02:47:44    192 dev
drwxr-xr-x    4 anon anon  -8917575-11-08 02:47:44    128 proc
drwxrwxrwx    5 anon anon  -8917575-11-08 02:47:44    128 lib
-rwxr-xr-x    7 anon anon  -8917575-11-08 02:47:44     43 test.txt
$ /bin/ls /bin
drwxrwxr-x    2 anon anon  -8917582-02-27 01:15:00    448 .
drwxrwxr-x    1 anon anon  -8917582-02-27 01:15:00   4096 ..
-rwxr-xr-x    8 anon anon  -8917575-11-06 09:50:08  41816 cat
-rwxr-xr-x    9 anon anon  -8917575-11-06 09:50:08  22312 init
-rwxr-xr-x   10 anon anon  -8917575-11-06 09:50:08  42720 echo
-rwxr-xr-x   11 anon anon  -8917575-11-06 09:50:08  57448 sh
-rwxr-xr-x   12 anon anon  -8917575-11-06 09:50:08  71512 ls
$ /bin/cat test.txt
test 123
abcdef
2025-02-20
123 test
abcdef
$ /bin/echo agc
agc
$
```
