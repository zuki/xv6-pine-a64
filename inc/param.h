#ifndef H_PARAM_H
#define H_PARAM_H
// カーネル(c, asm)とmkfsの両者でincludeされる

// -------------- 構成 -------------------------- //
#define NOFILE          16          // open files per process
#define NCPU            4           // # of cpu cores
#define MAXPATH         128         // maximum file path name
#define NINODE          50          // maximum number of active i-nodes
#define NDEV            10          // maximum major device number
#define MAXARG          32          // max exec arguments
#define NFILE           100         // open files per system
#define MAXOPBLOCKS     10          // max # of blocks any FS op writes fxl:too small?
#define NBUF            (MAXOPBLOCKS*3)     // size of disk block cache
#define LOGSIZE         1           // Max data blocks in on-disk
#define FSSIZE          7000        // ファイルシステムのブロック単位のサイズ
#define BLKSIZE         4096        // ブロックサイズ
#define SECTSIZE        512     // セクタサイズ（バイト）

#define DEV_RAMDISK     1       // ramdisk
#define DEV_VIRTDISK    2       // qemuのvirtioデバイス
// 以下はSDカード用（物理デバイスまたはエミュレートデバイス）
#define DEV_SD0         3       // partition0
#define DEV_SD1         4       // partition1
#define DEV_SD2         5       // partition2
#define DEV_SD3         6       // partition3
#define is_sddev(dev) (dev>=DEV_SD0 && dev<=DEV_SD3)

// ファイルシステムのルートディスクのデバイス番号。
// 注: ブロックディスクIDであり、major/minorではない
#define ROOTDEV         DEV_RAMDISK
#define SECONDDEV       DEV_SD1       // secondary dev to mount under /d/

// the malloc()/free() region which is carved out from the paging area
// #define MALLOC_PAGES  (16*1024*1024 / PAGE_SIZE)
#define MALLOC_PAGES    (8*1024*1024 / PAGE_SIZE)

// 多くのファイルで必要なので mmu.h から　ここに移した
#define VA_START    0x0FFFF000000000000
#define V2P(x)      ((unsigned long)x - VA_START)           // カーネル仮想アドレスから物理アドレスに変換
#define P2V(x)    ((void *)((unsigned long)x + VA_START)) // 物理アドレスからカーネル仮想アドレスに変換

#endif
