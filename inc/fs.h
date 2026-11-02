/*
 * On-disk file system format.
 * Both the kernel and user programs use this header file.
 */
#ifndef INC_FS_H
#define INC_FS_H

#include <types.h>
#include <param.h>
#include <linux/stat.h>
#include <linux/time.h>

// ディスク上のファイルシステムフォーマット.
// カーネル/ユーザプログラムの両者がこのヘッダーファイルを使用する.

#define ROOTINO             1       // ルートディレクトリ('/')のinode番号
#define BLKSECT             (BLKSIZE / SECTSIZE)

#define MAY_EXEC            1
#define MAY_WRITE           2
#define MAY_READ            4

#define RENAME_NOREPLACE    (1 << 0)    /* Don't overwrite target */
#define RENAME_EXCHANGE     (1 << 1)    /* Exchange source and dest */
#define RENAME_WHITEOUT     (1 << 2)    /* Whiteout source */

/* Disk layout:
 * [ boot block | super block | log | inode blocks | free bit map | data blocks ]
 *
 * mkfs computes the super block and builds an initial file system. The
 * super block describes the disk layout:
 */
struct superblock {
    uint32_t magic;        // Must be FSMAGIC
    uint32_t size;         // Size of file system image (blocks)
    uint32_t nblocks;      // Number of data blocks
    uint32_t ninodes;      // Number of inodes.
    uint32_t nlog;         // Number of log blocks
    uint32_t logstart;     // Block number of first log block
    uint32_t inodestart;   // Block number of first inode block
    uint32_t bmapstart;    // Block number of first free map block
};

#define FSMAGIC     0x10203040

#define NINDIRECT (BLKSIZE / sizeof(uint32_t))    // 128
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)   // (11 + 128 + 128 * 128) * 512 = 8261 KB

/* On-disk inode structure. */
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


/* Inodes per block. */
#define IPB           (BLKSIZE / sizeof(struct dinode))

/* Block containing inode i. */
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

/* Bitmap bits per block. */
#define BPB           (BLKSIZE*8)

/* Block of free map containing bit for block b. */
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

/* Directory is a file containing a sequence of dirent structures. */
#define DIRSIZ 58

struct dirent {
  uint32_t  inum;
  uint16_t  type;
  char name[DIRSIZ];
};

/* ファイルシステム独立なディレクトリエントリ */
struct dirent64 {
    ino64_t     d_ino;
    off64_t     d_off;
    uint16_t    d_reclen;
    uint8_t     d_type;
    char        d_name[256];
};

#define DESIZE      (sizeof(struct dirent))

#endif
