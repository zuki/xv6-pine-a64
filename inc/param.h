#ifndef H_PARAM_H
#define H_PARAM_H
// カーネル(c, asm)とmkfsの両者でincludeされる

// -------------- 構成 -------------------------- //
#define NOFILE          16      // open files per process
#define NCPU            4       // # of cpu cores
#define MAXPATH         128     // maximum file path name
#define NINODE          200     // maximum number of active i-nodes
#define MAXARG          32      // max exec arguments
#define MAXOPBLOCKS     10      // max # of blocks any FS op writes fxl:too small?
#define NBUF            (MAXOPBLOCKS*3)     // size of disk block cache
#define LOGSIZE         1           // Max data blocks in on-disk
#define FSSIZE          7000        // ファイルシステムのブロック単位のサイズ
#define BLKSIZE         4096        // ブロックサイズ
#define SECTSIZE        512         // セクタサイズ（バイト）
#define NDIRECT         11      // 直接指定のブロック数
#define NR_TASKS        32      // 128      // 32 should be fine. usertests.c seems to expect > 100
#define NR_MMS          NR_TASKS

// 起動時、カーネルの cprintf() とユーザによるコンソールへの書き込みを
// UART出力に加えてFBコンソール ("kconsole")にもリダイレクトする。
// kconsoleがアプリのUIに干渉するのを防ぐため、fb_init()の2回目以降の
// 呼び出しではkconsoleは無効化される。
// #define KCONSOLE_PRINTF        1        // デモ用でexperimental

// デバイス番号 : sutruct inode.dev
#define DEV_BOOT        0           // 起動ディスク(u-boot)
#define DEV_V6          1           // v6fs
#define DEV_FAT         2           // faffs
#define is_sddev(dev) (dev>=DEV_BOOT && dev<=DEV_V6)

// ファイルシステムのルートディスクのデバイス番号。
// 注: ブロックディスクIDであり、major/minorではない
#define ROOTDEV         DEV_V6
#define SECONDDEV       DEV_FAT       // secondary dev to mount under /d/

#define DEVFAT          makedev(0, 0)
#define DEVV6           makedev(0, 1)
#define DEVEXT2         makedev(0, 2)
#define DEVCONS         makedev(1, 0)

#define SDMAJOR         0       // major(DEVV6)
#define TTYMAJOR        1       // major(DEVCONS)

// the malloc()/free() region which is carved out from the paging area
// #define MALLOC_PAGES  (16*1024*1024 / PAGE_SIZE)
#define MALLOC_PAGES    (8*1024*1024 / PAGE_SIZE)

// ユーザ仮想メモリ
#define USER_BASE       0x400000
#define USER_TOP        0x0001000000000000 // == ユーザスタックのトップ。ユーザ仮想アドレス
#define USER_MAX_STACK  0x100000  // バイト単位、ページアライン (1MB)
#define MAX_TASK_USER_PAGES 0xFF000 // タスクあたりの最大ユーザページ数 : ELF_ET_DYN_BASE - USER_BASE
#define MAX_TASK_KER_PAGES  96      // タスクあたりの最大カーネルページ数  (128 seems to corrupt kernel stack

// 多くのファイルで必要なので mmu.h から　ここに移した
#define VA_START    0xFFFF000000000000
#define V2P(x)    ((unsigned long)(x) - VA_START)           // カーネル仮想アドレスから物理アドレスに変換
#define P2V(x)    ((void *)((unsigned long)(x) + VA_START)) // 物理アドレスからカーネル仮想アドレスに変換

#define V2P_WO(x) ((x) - VA_START)      /* Same as V2P, but without casts */
#define P2V_WO(x) ((x) + VA_START)      /* Same as P2V, but without casts */

#endif
