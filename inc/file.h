#ifndef INC_FILE_H
#define INC_FILE_H

#include <types.h>
#include <param.h>
#include <fs.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <sleeplock.h>
#include <procfs.h>
#ifdef CONFIG_FAT
#include <ff.h>
#endif

#define UIO_MAXIOV 1024

#define FILE_STATUS_FLAGS (O_APPEND|O_ASYNC|O_DIRECT|O_DSYNC|O_NOATIME|O_NONBLOCK|O_SYNC)
#define FILE_READABLE(flags) ((((flags) & O_ACCMODE) == O_RDWR) || (((flags) & O_ACCMODE) == O_RDONLY));
#define FILE_WRITABLE(flags) ((((flags) & O_ACCMODE) == O_RDWR) || (((flags) & O_ACCMODE) == O_WRONLY));

// in-memory copy of an inode
struct inode {
    uint32_t dev;               // Device number (cf ROOTDEV, not major/minor)
    uint32_t inum;              // Inode number
    int ref;                    // Reference count
    struct sleeplock lock;      // protects everything below here
    int valid;                  // inode has been read from disk?
#ifdef CONFIG_FAT
    FIL *fatfp;
    DIR *fatdir;
#endif
    short type;                 //
    short major;                // メジャーデバイス番号 (T_DEVICE only)
    short minor;                // マイナーデバイス番号 (T_DEVICE only)
    short nlink;                // inodeへのリンク数
    uint32_t size;              // ファイルサイズ（バイト単位）(bytes)
    mode_t mode;                // ファイルモード
#if 0
    uid_t uid;                  // 所有者のユーザーID
    gid_t gid;                  // 所有者のグループID
    struct timespec atime;      // 最新アクセス日時
    struct timespec mtime;      // 最新更新日時
    struct timespec ctime;      // 作成日時
#endif
    uint32_t addrs[NDIRECT+2];  // データブロックのアドレス
};

struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE, FD_PROCFS,FD_INODE_FAT } type;
    int     ref;
    char    readable;
    char    writable;
    char    nonblocking;    // FD_DEVICE only
    struct pipe   * pipe;   // FD_PIPE only
    struct inode  * ip;     // FD_INODE, FD_DEVICE (& FD_PROCFS)
    off_t   off;            // FD_INODE, FD_INODE_FAT
    short   major;          // FD_DEVICE (& FD_PROCFS)
    int     flags;          // ファイルステータス情報
    void    *content;       // ファイルに関するカーネル側の情報（ドライバのディスクリプタ、proffsコンテンツなど）
};

/*
 * Table mapping major device number to
 * device functions
 */
struct devsw {
    int (*read)(int user_dst, uint64_t dst, int off, int n, char blocking, void *content);
    int (*write)(int user_src, uint64_t src, int off, int n, void *content);
    struct termios *termios;
};

extern struct devsw devsw[];

void            fileinit();
struct file *   filealloc(void);
struct file *   filedup(struct file *f);
int             fileopen(char *path, int dirfd, int flags, mode_t mode);
void            fileclose(struct file *f);
int             filestat(struct file *f, uint64_t addr);
ssize_t         fileread(struct file *f, uint64_t addr, ssize_t n);
ssize_t         filewrite(struct file *f, uint64_t addr, ssize_t n);
off_t           filelseek(struct file *f, off_t offset, int whence);
int             filelink(char *oldpath, int olddirfd, char *newpath, int newdirfd);
int             fileunlink(char *path, int dirfd, int flags);
int             filesymlink(char *target, char *linkpath, int dirfd);
ssize_t         filereadlink(char *path, int dirfd, char *buf, size_t bufsize);
int             filerename(char *oldpath, int oldfd, char *newpath, int newfd);
int             filechmod(struct file *f, char *path, int dirfd, mode_t mode);
int             filemknod(char *path, mode_t mode, device_t dev);
int             filechdir(char *path);
int             fdalloc(struct file *f, int from);

#if 0
int             filechown(struct file *f, char *path, uid_t owner, gid_t group);
int             filepread64(struct file *f, void *buf, size_t count, off_t offset);

int             fsync(struct file *f, int type);
int             faccess(char *paht, int mode, int flags);
int             utimensat(char *path, struct timespec times[2]);

int             mount(char *source, char *target, char *fstype, uint64_t flags, void *data);
int             umount(char *target, int flags);
#endif

int             devnull_read(int user_dst, uint64_t dst, int off, int n, char blocking, void *content);
int             devnull_write(int user_src, uint64_t src, int off, int n, void *content);
int             devzero_read(int user_dst, uint64_t dst, int off, int n, char blocking, void *content);

#endif
