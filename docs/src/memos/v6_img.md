# v6ファイルシステムイメージを作成する

## ユーザコマンドの作成

```bash
$ make -C usr
make: Entering directory '/Volumes/wdb/pine64/xv6-pine64/usr'
make ../obj/usr/bin/cat ../obj/usr/bin/echo ../obj/usr/bin/sh ../obj/usr/bin/ls
make[1]: Entering directory '/Volumes/wdb/pine64/xv6-pine64/usr'
mkdir -p ../obj/usr/src/cat/
$(HOME)/musl/bin/musl-gcc -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 -fno-omit-frame-pointer -Iinc -c -o ../obj/usr/src/cat/main.c.o src/cat/main.c
mkdir -p ../obj/usr/bin/
$(HOME)/musl/bin/musl-gcc -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 -fno-omit-frame-pointer -Iinc -o ../obj/usr/bin/cat ../obj/usr/src/cat/main.c.o
aarch64-none-elf-objdump -S -d ../obj/usr/bin/cat > ../obj/usr/src/cat/cat.asm
aarch64-none-elf-objdump -x ../obj/usr/bin/cat > ../obj/usr/src/cat/cat.hdr
aarch64-none-elf-strip --strip-debug ../obj/usr/bin/cat
mkdir -p ../obj/usr/src/echo/
$(HOME)/musl/bin/musl-gcc -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 -fno-omit-frame-pointer -Iinc -c -o ../obj/usr/src/echo/main.c.o src/echo/main.c
mkdir -p ../obj/usr/bin/
$(HOME)/musl/bin/musl-gcc -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 -fno-omit-frame-pointer -Iinc -o ../obj/usr/bin/echo ../obj/usr/src/echo/main.c.o
aarch64-none-elf-objdump -S -d ../obj/usr/bin/echo > ../obj/usr/src/echo/echo.asm
aarch64-none-elf-objdump -x ../obj/usr/bin/echo > ../obj/usr/src/echo/echo.hdr
aarch64-none-elf-strip --strip-debug ../obj/usr/bin/echo
mkdir -p ../obj/usr/src/sh/
$(HOME)/musl/bin/musl-gcc -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 -fno-omit-frame-pointer -Iinc -c -o ../obj/usr/src/sh/main.c.o src/sh/main.c
mkdir -p ../obj/usr/bin/
$(HOME)/musl/bin/musl-gcc -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 -fno-omit-frame-pointer -Iinc -o ../obj/usr/bin/sh ../obj/usr/src/sh/main.c.o
aarch64-none-elf-objdump -S -d ../obj/usr/bin/sh > ../obj/usr/src/sh/sh.asm
aarch64-none-elf-objdump -x ../obj/usr/bin/sh > ../obj/usr/src/sh/sh.hdr
aarch64-none-elf-strip --strip-debug ../obj/usr/bin/sh
mkdir -p ../obj/usr/src/ls/
$(HOME)/musl/bin/musl-gcc -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 -fno-omit-frame-pointer -Iinc -c -o ../obj/usr/src/ls/main.c.o src/ls/main.c
mkdir -p ../obj/usr/bin/
$(HOME)/musl/bin/musl-gcc -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 -fno-omit-frame-pointer -Iinc -o ../obj/usr/bin/ls ../obj/usr/src/ls/main.c.o
aarch64-none-elf-objdump -S -d ../obj/usr/bin/ls > ../obj/usr/src/ls/ls.asm
aarch64-none-elf-objdump -x ../obj/usr/bin/ls > ../obj/usr/src/ls/ls.hdr
aarch64-none-elf-strip --strip-debug ../obj/usr/bin/ls
rm ../obj/usr/src/ls/main.c.o ../obj/usr/src/cat/main.c.o ../obj/usr/src/echo/main.c.o ../obj/usr/src/sh/main.c.o
make[1]: Leaving directory '/Volumes/wdb/pine64/xv6-pine64/usr'
make: Leaving directory '/Volumes/wdb/pine64/xv6-pine64/usr'

$ ls -l obj/usr/bin
total 220
-rwxr-xr-x 1 zuki staff 41816 10月 19 15:16 cat
-rwxr-xr-x 1 zuki staff 42720 10月 19 15:16 echo
-rwxr-xr-x 1 zuki staff 71512 10月 19 15:16 ls
-rwxr-xr-x 1 zuki staff 57448 10月 19 15:16 sh
```

## mkfsコマンドをビルドしてv6ファイルシステムイメージを作成する

```bash
$ make -f mksd.mk mkfs/mkfs
cc -o mkfs/mkfs -Imkfs/ mkfs/mkfs.c

$ make -f mksd.mk obj/v6.img
echo mkfs/mkfs test.txt obj/usr/bin/cat obj/usr/bin/echo obj/usr/bin/sh obj/usr/bin/ls
mkfs/mkfs test.txt obj/usr/bin/cat obj/usr/bin/echo obj/usr/bin/sh obj/usr/bin/ls
./mkfs/mkfs obj/v6.img test.txt obj/usr/bin/cat obj/usr/bin/echo obj/usr/bin/sh obj/usr/bin/ls
nmeta 11 (boot, super, log blocks 1 inode blocks 7, bitmap blocks 1) blocks 6989 total 7000
input: 'test.txt' -> 'test.txt'
input: 'obj/usr/bin/cat' -> 'cat'
input: 'obj/usr/bin/echo' -> 'echo'
input: 'obj/usr/bin/sh' -> 'sh'
input: 'obj/usr/bin/ls' -> 'ls'
balloc: first 74 blocks have been allocated
balloc: write bitmap block at sector 10
```

## v6.imgをSDカードに書き込む

```bash
=> mmc part

Partition Map for MMC device 0  --   Partition Type: DOS

Part    Start Sector    Num Sectors     UUID            Type
  1     40960           102400          d414f95e-01     0c              // FAT32
  2     143360          61440           d414f95e-02     83              // Linux FS
```

```bash
$ diskutil list

/dev/disk6 (external, physical):
   #:                       TYPE NAME                    SIZE       IDENTIFIER
   0:     FDisk_partition_scheme                        *31.3 GB    disk6
   1:             Windows_FAT_32 BOOT                    52.4 MB    disk6s1
   2:                      Linux                         31.5 MB    disk6s2
                    (free space)                         31.2 GB    -

$ sudo diskutil unmountDisk /dev/disk6
Password:
Unmount of all volumes on disk6 was successful

$ sudo dd if=obj/v6.img of=/dev/disk6 bs=512 seek=143360
56000+0 records in
56000+0 records out
28672000 bytes (29 MB, 27 MiB) copied, 8.74988 s, 3.3 MB/s

$ sudo diskutil unmountDisk /dev/disk6
Unmount of all volumes on disk6 was successful
```
