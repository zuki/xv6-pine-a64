#ifndef INC_FILE_H
#define INC_FILE_H

#include <types.h>
#include <linux/stat.h>
#include <types.h>
#include <sleeplock.h>
#include <fs.h>
#include <proc.h>
#include <linux/fcntl.h>

#define FILE_READABLE(file) ((((file->flags) & O_ACCMODE) == O_RDWR) || (((file->flags) & O_ACCMODE) == O_RDONLY))

#define FILE_WRITABLE(file) ((((file->flags) & O_ACCMODE) == O_RDWR) || (((file->flags) & O_ACCMODE) == O_WRONLY))


struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE, FD_PROCFS, FD_INODE_FAT } type;
    int ref;
    char readable;
    char writable;
    struct pipe *pipe;
    struct inode *ip;
    size_t off;
    uint16_t major;
    int flags;
};


/* In-memory copy of an inode. */
struct inode {
    uint32_t dev;               // Device number
    uint32_t inum;              // Inode number
    int ref;                    // Reference count
    struct sleeplock lock;      // Protects everything below here
    int valid;                  // Inode has been read from disk?

    uint16_t type;              // v6ファイルタイプ : T_XXX
    uint16_t nlink;             // inodeへのリンク数
    device_t devno;             // デバイス番号 : major/minor (T_DEVICE only)
    uint32_t size;              // ファイルサイズ（バイト単位）(bytes)
    mode_t   mode;              // ファイルモード

    struct timespec atime;      // 最新アクセス日時
    struct timespec mtime;      // 最新更新日時
    struct timespec ctime;      // 作成日時

    uint32_t addrs[NDIRECT+2];
};

/*
 * Table mapping major device number to
 * device functions
 */
struct devsw {
    ssize_t (*read)(int user_dst, uint64_t dst, int off, int n);
    ssize_t (*write)(int user_dst, uint64_t src, int off, int n);
};

extern struct devsw devsw[];

void            readsb(int dev, struct superblock *sb);
int             dirlink(struct inode *dp, char *name, uint32_t inum, uint16_t type);
struct inode *  dirlookup(struct inode *dp, char *name, size_t *poff);
struct inode *  ialloc(uint32_t dev, short type);
struct inode *  idup(struct inode *ip);
void            fsinit(int dev);
void            iinit(void);
void            ilock(struct inode *ip);
void            iput(struct inode *ip);
void            iunlock(struct inode *ip);
void            iunlockput(struct inode *ip);
void            iupdate(struct inode *ip);
int             namecmp(const char *s, const char *t);
struct inode *  namei(const char *path);
struct inode *  nameiparent(const char *path, char *name);
int             stati(struct inode *ip, uint64_t stp);
ssize_t         readi(struct inode *ip, int user_dst, char *dst, size_t off, size_t n);
ssize_t         writei(struct inode *ip, int user_src, char *src, size_t off, size_t n);

void            fileinit(void);
struct file *   filealloc(void);
struct file *   filedup(struct file *f);
void            fileclose(struct file *f);
int             filestat(struct file *f, uint64_t stp);
ssize_t         fileread(struct file *f, uint64_t addrp, ssize_t n, int user);
ssize_t         filewrite(struct file *f, uint64_t addrp, ssize_t n, int user);
off_t           filelseek(struct file *file, off_t offset, int whence);
int             filegetdents(struct file *f, uint64_t dirp, size_t size);
int             filewriteback(struct file *file, off_t offset, uint64_t addr);

int             pipealloc(struct file **f0, struct file **f1);
void            pipeclose(struct pipe *p, int writable);
ssize_t         pipewrite(struct pipe *p, char *addr, ssize_t n);
ssize_t         piperead(struct pipe *p, char *addr, ssize_t n);

#endif
