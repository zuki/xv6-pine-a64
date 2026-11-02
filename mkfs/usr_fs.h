/*
 * On-disk file system format.
 * Both the kernel and user programs use this header file.
 */
#ifndef MKFS_INC_FS_H
#define MKFS_INC_FS_H

#include <stdint.h>
#include <time.h>
#include "types.h"

#include "../inc/param.h"

#undef ROOTDEV
#define V6_ROOTINO      1       // Root i-number

#define FSMAGIC     0x10203040
#define NDIRECT     11
#define NINDIRECT   (BLKSIZE / sizeof(uint32_t))    // 第一間接指定のブロック数
#define NINDIRECT2  (NINDIRECT * NINDIRECT)       // 第二間接指定のブロック数
#define MAXFILE     (NDIRECT + NINDIRECT + NINDIRECT2)   // 1ファイルの最大ブロック数 - 11 + 1024 + 1,048,576 = 1,049,611 : * 4096 = 4100 MB

/* Disk layout:
 * [ boot block | super block | log | inode blocks | free bit map | data blocks ]
 *
 * mkfs computes the super block and builds an initial file system. The
 * super block describes the disk layout:
 */
struct superblock {
    uint32_t magic;        // マジックナンバー
    uint32_t size;         // ファイルシステムのサイズ（ブロック単位）
    uint32_t nblocks;      // データブロックの総数
    uint32_t ninodes;      // inodeの総数.
    uint32_t nlog;         // ログブロックの総数
    uint32_t logstart;     // 先頭のログブロックのブロック番号
    uint32_t inodestart;   // 先頭のinodeブロックのブロック番号
    uint32_t bmapstart;    // 先頭のビットマップブロックのブロック番号
};

/* On-disk inode構造体 : 128バイト */
struct dinode {
    uint16_t            type;       //  0: V6ファイルタイプ : T_XXX
    uint16_t            nlink;      //  2: ハードリンク数
    device_t            devno;      //  4:デバイス番号
    uint32_t            size;       //  8: Size of file (bytes)
    mode_t              mode;       // 12: file mode
    uid_t               uid;        // 16: owner's user id
    gid_t               gid;        // 20: owner's gropu id
    struct timespec     atime;      // 24: last accessed time
    struct timespec     mtime;      // 40: last modified time
    struct timespec     ctime;      // 56: created time
    uint32_t addrs[NDIRECT+2];      // 72: Data block addresses
    char             _dummy[4];     // 124:
};

/* Inodes per block: 4 */
#define IPB             (BLKSIZE / sizeof(struct dinode))

/* Block containing inode i. */
#define IBLOCK(i, sb)   ((i) / IPB + sb.inodestart)

/* Bitmap bits per block;  4096 = 0x1000 */
#define BPB             (BLKSIZE*8)

/* Block of free map containing bit for block b. */
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

/* Directory is a file containing a sequence of dirent structures. */
#define DIRSIZ 58

struct dirent {
  uint32_t  inum;
  uint16_t  type;
  char name[DIRSIZ];
};

/* v6/fatのファイルタイプ */
#define T_DIR           1   // ディレクトリ
#define T_FILE          2   // ファイル
#define T_DEVICE        3   // デバイス
#define T_PROCFS        4   // Procfsエントリ
#define T_FILE_FAT      5   // FATファイル
#define T_DIR_FAT       6   // FATディレクトリ
#define T_MOUNT         7   // マウントポイント
#define T_SYMLINK       8   // シンボリックリンク

#define makedev(x,y) ((((x) & 0xff) << 8) | \
        ((y) & 0xff))
#define major(x) (((x)>>8) & 0xff)
#define minor(x) ((x) & 0xff)

#define DEVFAT          makedev(0, 0)
#define DEVV6           makedev(0, 1)
#define DEVEXT2         makedev(0, 2)
#define DEVCONS         makedev(1, 0)
#define DEVNULL         makedev(2, 0)
#define DEVZERO         makedev(3, 0)
#define DEVPROCFSCPU    makedev(4, 0)
#define DEVPROCFSMEM    makedev(5, 0)

#define ROOTDEV         (minor(DEVV6))      // Device number of file system root disk

#define SDMAJOR         (major(DEVV6))
#define TTYMAJOR        (major(DEVCONS))

#endif
