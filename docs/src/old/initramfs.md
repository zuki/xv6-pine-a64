# Linuxの/boot/initrd.img-`uname -r`

```bash
$ mkdir -p /tmp/initrd && cd /tmp/initrd
$ unmkinitramfs /boot/initrd.img-`uname -r`
$ ls
early  early2  main
$ ls -l main
total 36
lrwxrwxrwx  1 vagrant vagrant    7 Sep 17 16:44 bin -> usr/bin
drwxr-xr-x  3 vagrant vagrant 4096 Sep 17 16:44 conf
drwxr-xr-x  2 vagrant vagrant 4096 Sep 17 16:44 cryptroot
drwxr-xr-x 12 vagrant vagrant 4096 Sep 17 16:44 etc
-rwxr-xr-x  1 vagrant vagrant 7284 Jul 25 18:54 init
lrwxrwxrwx  1 vagrant vagrant    7 Sep 17 16:44 lib -> usr/lib
lrwxrwxrwx  1 vagrant vagrant    9 Sep 17 16:44 lib32 -> usr/lib32
lrwxrwxrwx  1 vagrant vagrant    9 Sep 17 16:44 lib64 -> usr/lib64
lrwxrwxrwx  1 vagrant vagrant   10 Sep 17 16:44 libx32 -> usr/libx32
drwxr-xr-x  2 vagrant vagrant 4096 Sep  3 09:55 run
lrwxrwxrwx  1 vagrant vagrant    8 Sep 17 16:44 sbin -> usr/sbin
drwxr-xr-x 10 vagrant vagrant 4096 Sep 17 16:44 scripts
drwxr-xr-x 11 vagrant vagrant 4096 Sep 17 16:44 usr
drwxr-xr-x  4 vagrant vagrant 4096 Sep 17 16:44 var
$ find . -name run-init
./usr/bin/run-init
vagrant@ubuntu-bionic:/tmp/initrd/main$ file usr/bin/run-init
usr/bin/run-init: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=d834167e42f8a96b40ea856be5fb85a400897021, for GNU/Linux 3.2.0, stripped
```
