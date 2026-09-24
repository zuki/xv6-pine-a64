# 演習0: 開発環境構築

## pine64向けの変更

- UART0DRのアドレスを変更
- ロードアドレスを変更

```bash
$ git diff hello.c Makefile startup.s link.ld
diff --git a/simple/Makefile b/simple/Makefile
index 73b792a..6ef20a8 100644
--- a/simple/Makefile
+++ b/simple/Makefile
@@ -1,4 +1,4 @@
-GCCPREFIX := aarch64-linux-gnu-
+GCCPREFIX := /opt/homebrew/bin/aarch64-elf-

 CC := $(GCCPREFIX)gcc
 LD := $(GCCPREFIX)ld
@@ -26,15 +26,5 @@ hello.img: $(OBJ) Makefile
 %.o: %.c
        $(CC) $(CFLAGS) -c -o $@ $<

-run: hello.img
-       qemu-system-aarch64 -M raspi3 -kernel $< -nographic
-
-qemu-gdb: hello.img
-       qemu-system-aarch64 -M raspi3 -kernel $< -gdb tcp::1234 -nographic -S
-
-gdb:
-       # gdb-multiarch -n -x .gdbinit
-       aarch64-linux-gdb -x .gdbinit
-
 clean:
        $(RM) hello.img hello.elf $(OBJ)
\ No newline at end of file
diff --git a/simple/hello.c b/simple/hello.c
index 84a7dac..d06b0db 100644
--- a/simple/hello.c
+++ b/simple/hello.c
@@ -1,4 +1,4 @@
-volatile unsigned int *const UART0DR = (unsigned int *) 0x3F201000;
+volatile unsigned int *const UART0DR = (unsigned int *) 0x1c28000;

 void print_uart0(const char *s) {
     while (*s != '\0') {
diff --git a/simple/link.ld b/simple/link.ld
index 0901ce7..38b8dce 100644
--- a/simple/link.ld
+++ b/simple/link.ld
@@ -1,7 +1,7 @@
 ENTRY(_start)
 SECTIONS
 {
- . = 0x80000;
+ . = 0x42000000;
  .startup . : { startup.o(.text) }
  .text : { *(.text) }
  .data : { *(.data) }
```

## 実行

- u-bootから実行（u-bootの環境は構築済み）
- imageファイルはtftpで取り込む

```bash
U-Boot SPL 2023.10-rc4 (Oct 04 2023 - 14:04:19 +0900)
DRAM: 1024 MiB
Trying to boot from MMC1
NOTICE:  BL31: v2.9(debug):5f01b0b
NOTICE:  BL31: Built : 10:14:49, Sep  3 2023
NOTICE:  BL31: Detected Allwinner A64/H64/R18 SoC (1689)
NOTICE:  BL31: Found U-Boot DTB at 0x20a44d0, model: Pine64+
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


U-Boot 2023.10-rc4 (Oct 04 2023 - 14:04:19 +0900) Allwinner Technology

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
=> bootp 192.168.10.105:/private/tftpboot/hello.img     // tftpでimageファイルをロード
ethernet@1c30000 Waiting for PHY auto negotiation to complete....... done
BOOTP broadcast 1
BOOTP broadcast 2
DHCP client bound to address 192.168.10.114 (257 ms)
Using ethernet@1c30000 device
TFTP from server 192.168.10.105; our IP address is 192.168.10.114
Filename '/private/tftpboot/hello.img'.
Load address: 0x42000000
Loading: #
         1000 Bytes/s
done
Bytes transferred = 157 (9d hex)
=> go 0x42000000                                        // 実行
## Starting application at 0x42000000 ...
Hello world
```
