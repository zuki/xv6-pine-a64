# sd.img

## 作成時のmakefile

```
$(SD_IMG): $(BOOT_IMG) $(FS_IMG)
	dd if=/dev/zero of=$@ seek=$$(($(SECTORS) - 1)) bs=$(SECTOR_SIZE) count=1
	printf "                                                                \
	  $(BOOT_OFFSET), $$(($(BOOT_SECTORS)*$(SECTOR_SIZE)/1024))K, c,\n      \
	  $(FS_OFFSET), $$(($(FS_SECTORS)*$(SECTOR_SIZE)/1024))K, L,\n          \
	" | sfdisk $@
	dd if=$(BOOT_IMG) of=$@ seek=$(BOOT_OFFSET) conv=notrunc
	dd if=$(FS_IMG) of=$@ seek=$(FS_OFFSET) conv=notrunc
```

## 中身

```bash
$ fdisk -l sd.img 
Disk sd.img: 1 GiB, 1073741824 bytes, 2097152 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0xfcbad681

Device     Boot   Start     End Sectors  Size Id Type
sd.img1            2048  133119  131072   64M  c W95 FAT32 (LBA)
sd.img2          133120 1116159  983040  480M 83 Linux
sd.img3         1116160 2097151  980992  479M 83 Linux

$ python3 -c "print(2048*512)"
1048576
$ python3 -c "print(133120*512)"
68157440
$ sudo mount -oloop,offset=68157440 sd.img /mnt
mount: /mnt: wrong fs type, bad option, bad superblock on /dev/loop17, missing codepage or helper program, or other error.
$ python3 -c "print(1116160*512)"
571473920
$ sudo mount -oloop,offset=571473920 sd.img /mnt
$ ls /mnt
lost+found
$ sudo umount /mnt
$ sudo mount -oloop,offset=1048576 sd.img /mnt
$ ls /mnt
armstub8-rpi4.bin  config.txt     fixup4.dat    kernel8.img       start4cd.elf  start.elf
armstub8.S         COPYING.linux  fixup_cd.dat  LICENCE.broadcom  start4.elf
bootcode.bin       fixup4cd.dat   fixup.dat     Makefile          start_cd.elf
```