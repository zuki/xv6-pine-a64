#ifndef H_PARAM_H
#define H_PARAM_H
// カーネル(c, asm)とmkfsの両者でincludeされる

// -------------- 構成 -------------------------- //
#define NOFILE          16      // open files per process
#define NCPU            4       // # of cpu cores
#define MAXPATH         128     // maximum file path name
#define NINODE          50      // maximum number of active i-nodes
// #define NDEV            10   // maximum major device number
#define MAXARG          32      // max exec arguments
#define NFILE           100     // open files per system
#define MAXOPBLOCKS     10      // max # of blocks any FS op writes fxl:too small?
#define NBUF            (MAXOPBLOCKS*3)     // size of disk block cache
#define NR_TASKS        32      // 128      // 32 should be fine. usertests.c seems to expect > 100
#define NR_MMS          NR_TASKS

// 起動時、カーネルの printf() とユーザによるコンソールへの書き込みを
// UART出力に加えてFBコンソール ("kconsole")にもリダイレクトする。
// kconsoleがアプリのUIに干渉するのを防ぐため、fb_init()の2回目以降の
// 呼び出しではkconsoleは無効化される。
// #define KCONSOLE_PRINTF        1        // デモ用でexperimental

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

#define LOGSIZE         (MAXOPBLOCKS*3) // max data blocks in on-disk log
#define FSSIZE          15000 // 2000   // size of file system in blocks
// (32 * 1024)  ok, but results in a very large ramdisk...

// the malloc()/free() region which is carved out from the paging area
// #define MALLOC_PAGES  (16*1024*1024 / PAGE_SIZE)
#define MALLOC_PAGES    (8*1024*1024 / PAGE_SIZE)

// 大きなユーザ仮想メモリ。ただし、freecount()のためにusertestsは遅くなる
#define USER_VA_END     (128 * 1024 * 1024) // == ユーザスタックのトップ。128MBのユーザ仮想アドレス
#define USER_MAX_STACK  (1 * 1024 * 1024)  // バイト単位、ページアライン
#define MAX_TASK_USER_PAGES (USER_VA_END / PAGE_SIZE)   // タスクあたりの最大ユーザページ数
#define MAX_TASK_KER_PAGES  96      // タスクあたりの最大カーネルページ数  (128 seems to corrupt kernel stack

// 小さなユーザ仮想メモリ（問題なし: userstests,slider,etc. メモリ不足: liteNesなど)
// #define USER_VA_END         (4 * 1024 * 1024) // == user stack top
// #define USER_MAX_STACK      (1 * 1024 * 1024)  // in bytes, must be page aligned.
// #define MAX_TASK_USER_PAGES (USER_VA_END / PAGE_SIZE)      // max userpages per task,
// #define MAX_TASK_KER_PAGES  16       //max kernel pages per task.

// 多くのファイルで必要なので mmu.h から　ここに移した
#define VA_START    0x0
#define VA2PA(x)    ((unsigned long)x - VA_START)           // カーネル仮想アドレスから物理アドレスに変換
#define PA2VA(x)    ((void *)((unsigned long)x + VA_START)) // 物理アドレスからカーネル仮想アドレスに変換

#endif
