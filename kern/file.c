/* File descriptors */
#include <types.h>
#include <param.h>
#include <file.h>
#include <linux/errno.h>
#include <sched.h>
#include <fs.h>
#include <procfs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <vm.h>
#include <console.h>
#include <pipe.h>
#include <exec.h>
#include <clock.h>
#include <string.h>
#include <linux/stat.h>
//#include <linux/capability.h>

// デバイス操作関数
struct devsw devsw[NDEV];

// ファイルテーブル
struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

// ファイル機能を初期化する
void fileinit()
{
    initlock(&ftable.lock, "file");
    init_devfs();
}

// ファイル構造体を割り当てる
struct file *filealloc(void)
{
    struct file *f;

    acquire(&ftable.lock);
    for (f = ftable.file; f < ftable.file + NFILE; f++) {
        if (f->ref == 0) {
            f->ref = 1;
            release(&ftable.lock);
            return f;
        }
    }
    release(&ftable.lock);
    return 0;
}

/* file fのレファレンスカウンタを増分する. */
struct file *filedup(struct file *f)
{
    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("filedup");
    f->ref++;
    release(&ftable.lock);
    return f;
}

static struct inode *create(char *path, int fd, int16_t type, uint16_t major, uint16_t minor, mode_t mode)
{
    struct inode *ip, *dp;
    char name[DIRSIZ];
    //struct timespec ts;
    int error = -EINVAL;

    if ((dp = nameiparent(path, name, fd)) == 0)
        return (void *)-ENOENT;

    ilock(dp);

#if 0
    if (permission(dp, MAY_EXEC) < 0) {
        iunlockput(dp);
        return (void *)-EACCES;
    }
#endif

    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iunlockput(dp);
        ilock(ip);
        if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE)) {
            return ip;
        }
        iunlockput(ip);
        return (void *)-EEXIST;
    }

    if ((ip = ialloc(dp->dev, type)) == 0) {
        iunlockput(dp);
        return (void *)-ENOMEM;
    }

    ilock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    ip->mode  = mode/* & ~(myproc()->umask) */;
    ip->type  = type;
#if 0
    clock_gettime(0, CLOCK_REALTIME, &ts);
    ip->atime = ip->mtime = ip->ctime = ts;
    ip->uid   = myproc()->uid;
    ip->gid   = myproc()->gid;
#endif
    iupdate(ip);

    if (type == T_DIR) {
        if (dirlink(ip, ".", ip->inum, ip->type) < 0
         || dirlink(ip, "..", dp->inum, dp->type) < 0) {
            error = -ENOMEM;
            goto fail;
        }
    }

    if ((error = dirlnk(dp, name, ip->inum, ip->type)) < 0) {
        goto fail;
    }

    if (type == T_DIR) {
        dp->nlink++;
        iupdate(dp);
    }

    iunlockput(dp);
    return ip;

fail:
    ip->nlink = 0;
    iupdate(ip);
    iunlockput(ip);
    iunlockput(dp);
    return (void *)error;
}

// ファイルを開く
int fileopen(char *path, int fd, int flags, mode_t mode)
{
    struct inode *ip;
    struct file *f;
    char buf[512];
    int fd, n;
    int error;

    if (flags & O_CREAT) {
        ip = namei(path, fd);
        if (ip && flags & O_EXCL) {
            iput(ip);
            warn("O_CREAT && O_EXCL and  %s exists", path);
            return -EEXIST;
        } else if (!ip) {
            if ((long)(ip = create(path, fd, T_FILE, 0, 0, mode | S_IFREG)) < 0) {
                warn("cant create %s", path);
                return (int)ip;
            }
        } else {
            ilock(ip);
        }
    } else {
loop:
        if ((ip = namei(path, fd)) == 0) {
            warn("cant namei %s", path);
            return -ENOENT;
        }
        ilock(ip);
        if (ip->type == T_DIR && (flags & O_ACCMODE) != 0) {
            iunlockput(ip);
            warn("wrong flags 0x%llx", flags);
            return -EINVAL;
        }
        if (ip->type == T_SYMLINK) {
            if ((n = readi(ip, 0, (uint64_t)buf, 0, sizeof(buf) - 1)) <= 0) {
                iunlockput(ip);
                warn("couldn't read sysmlink target");
                return -ENOENT;
            }
            buf[n] = 0;
            path = buf;
            iunlockput(ip);
            goto loop;
        }
    }
    int readable = FILE_READABLE((int)flags);
    int writable = FILE_WRITABLE((int)flags);
    if (readable && ((error = permission(ip, MAY_READ)) < 0))
        goto bad;
    if (writable && ((error = permission(ip, MAY_WRITE)) < 0))
        goto bad;
    if ((f = filealloc()) == 0 || (fd = fdalloc(f, 0)) < 0) {
        iunlock(ip);
        if (f) fileclose(f);
        warn("cant alloc file\n");
        return -ENOSPC;
    }

    if (ip->type == T_DEVICE) {
        f->type  = FD_DEVICE;
        f->major = ip->major;
        f->off   = 0;
        f->nonblocking = (mode & O_NONBLOCK) ? 1 : 0;
    } else if (ip->type == T_PROCFS) {
        f->type  = FD_PROCFS;
        f->major = ip->major;
        f->off   = 0;
        f->content = kalloc(1);
        assert(f->content);
        struct procfs_state *st = f->content;
        procfs_init_state(st);
        if ((st->ksize = procfs_gen_content(f->major, st->kbuf, MAX_PROCFS_SIZE)) < 0)
            panic("gen_content");
    } else {
        f->type = FD_INODE;
        f->off  = (flags & O_APPEND) ? ip->size : 0;
    }


    f->ip       = ip;
    f->flags    = flags;
    f->readable = readable;
    f->writable = writable;
#if 0
    if (flags & O_CLOEXEC)
        bit_add(myproc()->fdflag, fd);
#endif

    debug("proc[%d], path: %s, fd: %d, flags: %d, mode: 0x%x", myproc()->pid, path, fd, f->flags, f->ip->mode);
    iunlock(ip);
    return fd;

bad:
    iunlockput(ip);
    return -EACCES;
}

// ファイル fを閉じる（レファレンスカウンタを減じて、0になったら閉じる）
void fileclose(struct file *f)
{
    struct file ff;

    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0) {
        release(&ftable.lock);
        return;
    }
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release(&ftable.lock);

    if (ff.type == FD_PROCFS && ff.content) {
        struct procfs_state *st = (struct procfs_state *)(ff.content);
        if (st->usize)
            procfs_parse_usercontent(&ff);
        kfree(ff.content);
    } else if (ff.type == FD_PIPE) {
        pipeclose(ff.pipe, ff.writable);
    } else if (ff.type == FD_INODE || ff.type == FD_DEVICE || ff.type == FD_PROCFS) {
        iput(ff.ip);
    }

#ifdef CONFIG_FAT
    else if (ff.type == FD_INODE_FAT) {
        if (ff.ip->fatfp) {
            f_close(ff.ip->fatfp);
            free(ff.ip->fatfp);
        } else if (ff.ip->fatdir) {
            f_closedir(ff.ip->fatdir);
            free(ff.ip->fatdir);
        }
        iput(ff.ip);
    }
#endif
}

// ファイルfに関するメタデータを取得する
int filestat(struct file *f, uint64_t addr)
{
    struct task_struct *p = myproc();
    struct stat st;
    int error;

    if (f->type == FD_INODE  || f->type == FD_DEVICE
     || f->type == FD_PROCFS || f->type == FD_INODE_FAT) {
        ilock(f->ip);
        stati(f->ip, &st);
        iunlock(f->ip);
        if ((error = copyout(p->mm, addr, (char *)&st, sizeof(st))) < 0) {
            return error;
        }
        return 0;
    }
    return -EFAULT;
}

static int procfs_read(struct file *f, uint64_t dst, uint32_t n);

/* ファイルfを読み取る */
ssize_t fileread(struct file *f, uint64_t addr, ssize_t n)
{
    ssize_t r = 0;

    if (f->type == FD_PIPE) {
        r = piperead(f->pipe, addr, n);
    } else if (f->type == FD_DEVICE) {
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
            return -EBADF;
        r = devsw[f->major].read(1, addr, 0, n, !f->nonblocking, f->content);
    } else if (f->type == FD_INODE) {
        ilock(f->ip);
        if ((r = readi(f->ip, 1, addr, f->off, n)) > 0)
            f->off += r;
        //clock_gettime(CLOCK_REALTIME, &f->ip->atime);
        iunlock(f->ip);
    }
#ifdef CONFIG_FAT
    else if (f->type == FD_INODE_FAT) {
        if (f->ip->type == T_FILE_FAT) {
            char *buf = malloc(n);
            if (!buf)
                return -EIO;
            uint32_t n1;
            ilock_fat(f->ip);
            if ((r = f_read(f->ip->fatfp, buf, n, &n1)) == FR_OK
              && either_copyout(1, addr, buf, n1) == 0) {
                f->off += n1;
                r = (ssize_t)n1;
            } else {
                error("f_read failed returns %d, n=%d", r, n);
                return -1;
            }
            iunlock(f->ip);
            free(buf);
        } else if (f->ip->type == T_DIR_FAT) {
            int sz = sizeof(FILINFO);
            if (n < sz) {
                error("n %ld is smaller than sz %ld", n, sz);
                r = -EINVAL;
            } else {
                FILINFO fno;
                assert(f->ip->fatdir);
                if (f_readdir(f->ip->fatdir, &fno) == FR_OK) {
                    if (fno.fname[0]) {
                        if (either_copyout(1, addr, &fno, sz) == 0) {
                            r = sz;
                        } else {
                            error("either_copyout failed");
                            r = -EINVAL;
                        }
                    } else {
                        r = 0;
                    }
                } else {
                    error("f_readdir failed");
                    r = -EINVAL;
                }
            }
        } else {
            error("invalid type %d", f->ip->type);
            r = -EINVAL;
        }
    }
#endif
    else if (f->type == FD_PROCFS) {
        return procfs_rea(f, addr, n);
    } else {
        error("invalid file type: %d", f->type);
        r = -EINVAL;
    }

    return r;
}

#if 0
int filepread64(struct file *f, void *buf, size_t count, off_t offset)
{
    long error;
    off_t old_off;
    ssize_t n;

    if ((old_off = filelseek(f, 0, SEEK_CUR)) < 0)
        return old_off;
    if ((error = filelseek(f, offset, SEEK_SET)) < 0)
        return error;
    n = fileread(f, (char *)buf, count);
    if ((error = filelseek(f, old_off, SEEK_SET)) < 0)
        return error;
    return n;
}
#endif

/* ファイル fに書き込む. */
ssize_t filewrite(struct file *f, uint64_t addr, ssize_t n)
{
    ssize_t r = 0;
    //struct timespec ts;

    if (f->writable == 0)
        return -EBADF;
    if (f->type == FD_PIPE) {
        r = pipewrite(f->pipe, addr, n);
    } else if (f->type == FD_DEVICE) {
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
            return -EINVAL;
        r = devsw[f->major].write(1, addr, f->off, n, f->content);
    } else if (f->type == FD_INODE) {
        /*
         * 最大ログトランザクションサイズを超えないように、
         * 一度に数ブロックずつ書き込む。対象ブロックには
         * i-node、間接ブロック、アロケーションブロック、
         * 非アライン書き込み用の2ブロックのスロップがある。
         * writei() はコンソールのようなデバイスに書き込む
         * 場合もあるのでこれは本当に最下層のことです。
         */
        ssize_t max = ((MAXOPBLOCKS - 1 - 2 - 2) / 2) * 512;
        ssize_t i = 0;
        while (i < n) {
            ssize_t n1 = MIN(max, n - i);
            ilock(f->ip);
            if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
#if 0
             && (f->ip->type == T_FILE)) {
                update_page(f->off, f->ip->inum, f->ip->dev, addr + i, r);
                f->off += r;
             }
            clock_gettime(CLOCK_REALTIME, &ts);
            f->ip->mtime = f->ip->atime = ts;
#endif
            iunlock(f->ip);

            if (r < 0) break;
            if (r != n1)
                panic("short filewrite: r=%ld, n1=%ld", r, n1);
            i += r;
        }
        r = (i == n) ? n : -EIO;
    }
#ifdef CONFIG_FAT
    else if (f->type == FD_INODE_FAT) {
        char *buf = malloc(n);
        if (!buf)
            return -EIO;
        uint32_t n1;
        FRESULT fr = 9999;
        ilock_fat(f->ip);
        if (either_copyin(buf, 1, addr, n) == 0
        && (fr = f_write(f->ip->fatfp, buf, n, &n1)) == FR_OK) {
            if (n != n1)
                warn("f_write ok but disk is full");
            f->off += n1;
            r = n1;
        } else {
            error("f_write returns %d", fr);
            r = -EINVAL;
        }
        iunlock(f->ip);
        free(buf);
    }
#endif
    else if (f->type == FD_PROCFS) {
        r = procfs_write(f, addr, n);
    } else {
        error("invalid file type: %d", f->type);
        r = -EINVAL;
    }

    return r;
}

off_t filelseek(struct file *f, off_t offset, int whence)
{
    int newoff;
    uint32_t size = 0;

    if (!f) return -EBADF;

    if (f->type == FD_PIPE || f->type == FD_PROCFS)
        return -EINVAL;
    if (f->type == FD_DEVICE) {
        if (f->major < 0 || f->major >= NDEV)
            return -EINVAL;
    }

#ifdef CONF_FAT
    if (f->type == FD_INODE_FAT) {
        int ret = 0;
        ilock(f->ip);
        size = f->ip->size;
        switch (whence) {
        case SEEK_SET:
            if (offset > size) {
                warn("seek beyond file end. will expand file");}
            if (offset < 0)
                ret = -1;
            break;
        case SEEK_CUR:
            if (offset < 0 && -offset > f->off) {
                error("bad offset %d", offset);
                ret = -1;
            } else {
                offset = f->off + offset;
                if (offset > size) {
                    warn("warning: seek beyond file end. will expand file");
                }
            }
            break;
        case SEEK_END:
            if (offset < 0 && -offset > size) {
                error("bad offset %d", offset);
                ret = -1;
            } else {
                offset = size + offset;
                if (offset>size) {
                    warn("warning: seek beyond file end. will expand file");
                }
            }
            break;
        default:
            error("unrecog option");
            ret = -1;
        }

        if (ret != -1) {
            if ((ret = f_lseek(f->ip->fatfp, ofset) != FR_OK)) {
                error("f_lseek failed with %d", ret);
                ret = -EINVAL;
            } else {
                ret = offset;
                f->off = offset;
            }
        }
        iunlock(f->ip);

        return (ret == -1) ? -EINVAL : ret;
    }
#endif
    if (f->type == FD_INODE) {
        ilock(f->ip);
        size = f->ip->size;
        iunlock(f->ip);
        switch(whence) {
            case SEEK_SET:
                if (offset < 0)
                    goto bad;
                else
                    f->off = offset;
                break;
            case SEEK_CUR:
                if (f->off + offset < 0)
                    goto bad;
                else
                    f->off += offset;
                break;
            case SEEK_END:
                if (size + offset < 0)
                    goto bad;
                else
                    f->off = size + offset;
                break;
            default:
                goto bad;
        }
        return f->off;
    } else {
        error("invalid file type: %d", f->type);
        return -EINVAL;
    }

bad:
    debug("invalid offset %d", offset)
    return -EINVAL;
}

int filelink(char *oldpath, int oldfd, char *newpath, int newfd)
{
    char name[DIRSIZ];
    struct inode *dp, *ip;
    int error;

    begin_op();
    if ((ip = namei(oldpath, oldfd)) == 0) {
        return -ENOENT;
    }

    ilock(ip);
    if (ip->type == T_DIR) {
        iunlockput(ip);
        return -EPERM;
    }

    ip->nlink++;
    iupdate(ip);
    iunlock(ip);

    if ((dp = nameiparent(newpath, name, newfd)) == 0) {
        error = -ENOENT;
        goto bad;
    }

    ilock(dp);
    if (dp->dev != ip->dev) {
        error = -EXDEV;
        iunlockput(dp);
        goto bad;
    }

    if ((error = dirlink(dp, name, ip->inum, ip->type)) != 0) {
        iunlockput(dp);
        goto bad;
    }

    iunlockput(dp);
    iput(ip);
    return 0;

bad:
    ilock(ip);
    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);
    return error;
}

int filesymlink(char *target, char *linkpath, int fd)
{
    char name[DIRSIZ];
    struct inode *dp, *ip;
    int error;
    //struct timespec ts;

    if ((dp = nameiparent(linkpath, name, fd)) == 0) {
        return -ENOENT;
    }

    ilock(dp);
    if ((ip = ialloc(dp->dev, T_SYMLINK)) == 0) {
        iunlockput(dp);
        return -ENOMEM;
    }

    ilock(ip);
    ip->major = 0;
    ip->minor = 0;
    ip->nlink = 1;
    ip->mode = S_IFLNK | 0777;
    ip->type = T_SYMLINK;
#if 0
    clock_gettime(CLOCK_REALTIME, &ts);
    ip->atime = ip->mtime = ip->ctime = ts;
#endif
    iupdate(ip);

    if ((error = dirlink(dp, name, ip->inum, ip->type)) != 0) {
        iunlockput(dp);
        iunlockput(ip);
        return error;
    }

    iupdate(dp);
    iunlockput(dp);

    writei(ip, 0, (uint64_t)target, 0, strlen(target));
    iupdate(ip);
    iunlockput(ip);
    return 0;
}

ssize_t filereadlink(char *path, int fd, char *buf, size_t bufsize)
{
    struct inode *ip, *dp;
    ssize_t n;
    char name[DIRSIZ];

    if ((dp = nameiparent(path, name, fd)) == 0) {
        return -ENOTDIR;
    } else {
        iput(dp);
    }

    if ((ip = namei(path, fd)) == 0) {
        return -ENOENT;
    }

    ilock(ip);
    if (ip->type != T_SYMLINK) {
        iunlockput(ip);
        return -EINVAL;
    }

    if ((n = readi(ip, 1, buf, 0, bufsize)) <= 0) {
        iunlockput(ip);
        return -EIO;
    }

    iunlockput(ip);
    return n;
}

int fileunlink(char *path, int fd, int flags)
{
    struct inode *ip, *dp;
    char name[DIRSIZ];
    size_t off;
    int error;

    if ((dp = nameiparent(path, name, fd)) == 0) {
        return -ENOENT;
    }

    ilock(dp);

    /* Cannot unlink "." or "..". */

    if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
        error = -EPERM;
        goto baddp;
    }

    if ((ip = dirlookup(dp, name, &off)) == 0) {
        error = -ENOENT;
        goto baddp;
    }

    ilock(ip);
    error = iunlink(dp, ip, flags);

badip:
    iunlockput(ip);
baddp:
    iunlockput(dp);

    return error;
}

/*
 * Allocate a file descriptor for the given file.
 * Takes over file reference from caller on success.
 */
int fdalloc(struct file *f, int from)
{
    struct task_struct *p = myproc();

    for (int fd = from; fd < NOFILE; fd++) {
        if (p->ofile[fd] == 0) {
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -EBADF;
}

int filemknod(char *path, mode_t mode, device_t dev)
{
    struct inode *ip;
    uint16_t major, minor;
    int error;

    if (!S_ISCHR(mode) && !S_ISBLK(mode)) {
        error("bad type: 0x%lx", mode & S_IFMT);
        return -EINVAL;
    }

    major = (uint16_t)((dev >> 8) & 0xFF);
    minor = (uint16_t)(dev & 0xFF);

    if ((long)(ip = create(path, AT_FDCWD, T_DEVICE, major, minor, mode)) < 0) {
        error("error: 0x%lx", (long)ip);
        return (int)ip;
    }
    trace("created: ip=%d", ip->inum);
    iunlockput(ip);
    return 0;
}

int filechdir(char *path)
{
    struct inode *ip;
    struct task_struct *p = myproc();
    int error;

    if ((error = namei(path, AT_FDCWD)) < 0)
        return error;
    ilock(ip);
    if (ip->type != T_DIR) {
        iunlockput(ip);
        return -ENOTDIR;
    }
    iunlock(ip);
    iput(p->cwd);
    p->cwd = ip;
    return 0;
}

#if 0
int utimensat(char *path, struct timespec times[2])
{
    struct inode *ip, *dp;
    char name[DIRSIZ];
    size_t off;
    struct timespec ts;

    begin_op();
    if ((dp = nameiparent(path, name)) == 0) {
        end_op();
        return -ENOENT;
    }

    ilock(dp);
    if ((ip = dirlookup(dp, name, &off)) == 0) {
        iunlockput(dp);
        end_op();
        return -ENOENT;
    }
    iunlockput(dp);

    ilock(ip);
    // TODO: 権限チェックが必要
    if (times == NULL) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ip->atime = ip->mtime = ts;
    } else {
        for (int i = 0; i < 2; i++) {
            if (times[i].tv_nsec == UTIME_NOW) {
                clock_gettime(CLOCK_REALTIME, &ts);
                if (i == 0) ip->atime = ts;
                else        ip->mtime = ts;
            } else if (times[i].tv_nsec == UTIME_OMIT) {
                // noop
            } else {
                if (i == 0) ip->atime = times[i];
                else        ip->mtime = times[i];
            }
        }
    }
    iupdate(ip);
    iunlockput(ip);
    end_op();
    return 0;
}
#endif

int filechmod(struct file *f, char *path, int fd, mode_t mode)
{
    struct inode *ip;

    if (f != NULL) {
        ip = f->ip;
        idup(ip);
    } else {
        if ((ip = namei(path, fd)) == 0) {
            return -ENOENT;
        }
    }

    ilock(ip);
    ip->mode = (ip->mode & S_IFMT) | mode;
    iupdate(ip);
    iunlockput(ip);

    return 0;
}

#if 0
int filechown(struct file *f, char *path, uid_t owner, gid_t group)
{
    struct inode *ip;
    struct task_struct *p = myproc();
    long error = -EPERM;
    int i;

    begin_op();
    if (f != NULL) {
        ip = f->ip;
    } else {
        if ((ip = namei(path)) == 0) {
            end_op();
            return -ENOENT;
        }
    }

    ilock(ip);

    if (owner != (uid_t)-1) {
        if (!capable(CAP_CHOWN)) {
            debug("uid %d cant chown", owner);
            goto bad;
        }
        ip->uid = owner;
    }

    if (group != (gid_t)-1) {
        if (capable(CAP_CHOWN)) {
            ip->gid = group;
        } else if (ip->uid == p->euid) {
            for (i = 0; i < p->ngroups; i++) {
                if (p->groups[i] == group) {
                    ip->gid = group;
                    break;
                }
            }
            if (i == p->ngroups) {
                debug("uid %d and gid %d cant chown", owner, group);
                goto bad;
            }
        } else {
            debug("gid %d cant chown", group);
            goto bad;
        }
    }

    if (ip->mode & S_IXUGO && !capable(CAP_CHOWN)) {
        ip->mode &= ~(S_ISUID|S_ISGID);
    }
    iupdate(ip);
    error = 0;

bad:
    iunlockput(ip);
    end_op();
    return error;
}

int faccess(char *path, int mode, int flags)
{
    struct inode *ip;
    struct task_struct *p = myproc();
    uid_t uid, fuid;
    gid_t gid, fgid;
    mode_t fmode;

    if (mode & ~S_IRWXO)    /* where's F_OK, X_OK, W_OK, R_OK? */
        return -EINVAL;

    begin_op();
    if ((ip = namei(path)) == 0) {
        end_op();
        return -ENOENT;
    }
    fmode = ip->mode;
    fuid = ip->uid;
    fgid = ip->gid;
    iput(ip);
    end_op();

    // mode == F_OKの場合はファイルが存在するのでOK
    if (mode == 0) return 0;

    // 呼び出し元がrootならR_OK, W_OKは常にOK
    // X_OKはファイルにUGOのいずれかに実行許可があればOK
    if (p->uid == 0) {
        if ((mode & X_OK) && !(fmode & S_IXUGO))
            return -EACCES;
        else
            return 0;
    }

    // root以外は個別に判断
    if (flags & AT_EACCESS) {       // 実効IDで判断
        uid = p->euid;
        gid = p->egid;
    } else {                        // 実IDで判断
        uid = p->uid;
        gid = p->gid;
    }

    if (mode & R_OK) {
        if ((fuid == uid && !(fmode & S_IRUSR))
         && (fgid == gid && !(fmode & S_IRGRP))
                         && !(fmode & S_IROTH))
        return -EACCES;
    }

    if (mode & W_OK) {
        if ((fuid == uid && !(fmode & S_IWUSR))
         && (fgid == gid && !(fmode & S_IWGRP))
                         && !(fmode & S_IWOTH))
        return -EACCES;
    }

     if (mode & X_OK) {
        if ((fuid == uid && !(fmode & S_IXUSR))
         && (fgid == gid && !(fmode & S_IXOTH))
                         && !(fmode & S_IXOTH))
        return -EACCES;
    }

    return 0;
}
#endif

// dp/name1 -> dp/name2 へ改名
static int rename(struct inode *dp, char *name1, char *name2)
{
    struct inode *ip;
    struct dirent de;
    size_t off;

    ilock(dp);
    if ((ip = dirlookup(dp, name1, &off)) == 0)
        return -ENOENT;
    ilock(ip);

    memset(&de, 0, sizeof(de));
    de.inum = ip->inum;
    de.type = ip->type;
    memmove(de.name, name2, DIRSIZ);
    if (writei(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
        warn("writei");
        return -ENOSPC;
    }
    iupdate(dp);
    //clock_gettime(CLOCK_REALTIME, &ip->ctime);
    iupdate(ip);
    iunlockput(ip);
    iunlockput(dp);

    return 0;
}

// dp/old_ip -> dp/new_ip へ付け替え
static int reinode(struct inode *dp, struct inode *old_ip, struct inode *new_ip, char *name)
{
    struct dirent de;
    size_t off;
    //struct timespec ts;
    int error;

    ilock(dp);
    old_ilock(old_ip);
    new_ilock(new_ip);
    if (direntlookup(dp, old_ip->inum, &de, &off) < 0) {
        error = -ENOENT;
        goto bad;
    }

    de.inum = new_ip->inum;
    de.type = new_ip->type;
    strncpy(de.name, name, DIRSIZ);
    if (writei(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
        trace("writei");
        error = -ENOSPC;
        goto bad;
    }
    iupdate(dp);
#if 0
    clock_gettime(CLOCK_REALTIME, &ts);
    old_ip->ctime = new_ip->ctime = ts;
#endif
    iupdate(old_ip);
    iupdate(new_ip);
    error = iunlink(dp, old_ip, old_ip->type == T_DIR ? AT_REMOVEDIR : 0);

bad:
    iunlockput(new_ip);
    iunlockput(old_ip);
    iunlockput(dp);

    return error;
}

int filerename(char *oldpath, int oldfd, char *newpath, int newfd, uint32_t flags)
{
    struct inode *ip1, *ip2, *dp1, *dp2;
    char name1[DIRSIZ], name2[DIRSIZ];
    int error;

    if (flags & RENAME_NOREPLACE) {
        if ((ip2 = namei(newpath, newfd)) != 0) {
            iput(ip2);
            error("RENAME_NOREPLACE & newpath exist");
            return -EEXIST;
        }
    }

    if ((ip1 = namei(oldpath, oldfd)) == 0) {
        warn("oldpath %s not exits", oldpath);
        return -ENOENT;
    }
    dp1 = nameiparent(oldpath, name1, oldfd);
    ip2 = namei(newpath, newfd);
    dp2 = nameiparent(newpath, name2, newfd);
    trace("oldpath: %s, dp1: %d, ip1: %d, name1: %s", oldpath, dp1->inum, ip1->inum, name1);
    trace("newpath: %s, dp2: %d, ip2: %d, name2: %s", newpath, dp2 ? dp2->inum : -1, ip2 ? ip2->inum : -1, name2);

    // 同一ファイルのhard link
    if (ip1 == ip2) {
        iput(ip1);
        iput(ip2);
        iput(dp1);
        iput(dp1);
        return 0;
    }

    error = -EINVAL;
    // 親ディレクトリが同じ
    if (dp1 == dp2) {
        // name2はなし: 単なる改名(name1はdirectoryでもfileでも可)
        if (ip2 == 0) {
rename1:
            if ((error = rename(dp1, name1, name2)) < 0) {
                warn("rename failed");
                goto bad;
            }
            iput(ip1);
            iput(dp2);
        // name2あり: direentを付け替えて、ip2はunlink
        } else {
            ilock(dp2);
            ilock(ip2);
            if ((error = iunlink(dp2, ip2, 0)) < 0) {
                warn("reinode failed");
                goto bad;
            }
            iunlockput(ip2);
            iunlock(dp2);
            goto rename1;
        }
    // 異なるディレクトリへのmove
    } else {
        if (ip2 == 0) {
            if ((error = dirlink(dp2, name2, ip1->inum, ip1->type)) < 0) {
                warn("dirlink failed 2");
                iput(dp1);
                iput(ip1);
                goto bad;
            }
            flags = ip1->type == T_DIR ? AT_REMOVEDIR : 0;
            iput(dp1);
            iput(ip1);
            return fileunlink(oldpath, oldfd, flags);
        } else {
            error = reinode(dp2, ip2, ip1, name2);
            iput(dp1);
        }
    }
bad:
    return error;
}

#if 0
int mount(char *source, char *target, char *fstype, uint64_t flags, void *data)
{
    struct inode *ip, *devi = 0;
    struct filesystem_type *fs_t;
    long error;
    info("source: %s, target: %s, type: %s, flags: 0x%llx", source, target, fstype, flags);
    begin_op();
    if ((ip = namei(target)) == 0 || (devi = namei(source)) == 0) {
        warn("not found: ip=0x%p, devi=0x%p", ip, devi);
        end_op();
        return -ENOENT;
    }

    // 1: fstypeがサポートされているかチェック
    if ((fs_t = getfs(fstype)) == 0) {
        warn("fstype %s not found", fstype);
        return -ENODEV;
    }

    error = -ENOTDIR;
    ilock(ip);
    devi->iops->ilock(devi);

    // 2: マウントポイント ipの妥当性チェック: directoryであること
    if (ip->type != T_DIR && ip->ref > 1) {
        warn("target %s is not dir", target);
        goto bad;
    }

    // 3: 被マウント deviの妥当性チェック: デバイスファイルであること
    error = -ENOTBLK;
    if (devi->type != T_DEV) {
        warn("source %s is not dev", source);
        goto bad;
    }

    // 4: デバイスのオープン（当面SDカードのみで常時オープンのためチェックしない）
/*
    error = -EINVAL;
    if (bdev_open(devi) != 0) {
        warn("source could not opened\n");
        goto bad;
    }
*/
    // 5: デバイスファイルの妥当性: VFATでもルートデバイスでもないこと
    if (devi->minor == FATMINOR || devi->minor == ROOTDEV) {
        warn("dev %d is vfat or rootdev", devi->minor);
        goto bad;
    }

    // 6: vfsリストにデバイスとファイルシステムを登録
    // Add this to a list to retrieve the filesystem type to current device
    if (putvfsonlist(devi->major, devi->minor, fs_t) == -1) {
        warn("failed to add dev to list");
        goto bad;
    }

    // 7: ファイルシステム固有のマウント操作
    if (fs_t->ops->mount(devi, ip) != 0) {
        warn("failed to mount\n");
        goto bad;
    }

    // 8: マウントポイントのファイルタイプを変更
    ip->type = T_MOUNT;
    //iupdate(ip);    // これを活かすとconsoleでcntl-Dが効かなくなる
    devi->iops->iunlock(devi);
    iunlock(ip);
    end_op();
    return 0;

bad:
    iunlockput(devi);
    iunlockput(ip);
    end_op();
    return error;
}

int umount(char *target, int flags)
{
    struct inode *ip, *devi;
    struct vfs *vfs;
    long error;

    begin_op();
    if ((devi = namei(target)) == 0) {
        warn("target %s is not found", target);
        end_op();
        return -ENOENT;
    }

    devi->iops->ilock(devi);
    if ((ip = mtablemntinode(devi)) == 0) {
        warn("could not get device ip");
        error = -EINVAL;
        goto bad2;
    }
    ilock(ip);

    if ((vfs = getvfsentry(SDMAJOR, devi->dev)) == 0) {
        warn("could not get vfsentry");
        error = -ENOENT;
        goto bad1;
    }

    if (vfs->fs_t->ops->unmount(devi) < 0) {
        warn("could not unmount");
        error = -EINVAL;
        goto bad1;
    }

    if ((error = deletefsfromlist(vfs)) < 0) {
        warn("error on delete fs from mlist");
        goto bad1;
    }

    devi->ref--;
    ip->type = T_DIR;

    error = 0;

bad1:
    iunlockput(ip);
bad2:
    iunlockput(devi);
    end_op();
    return error;
}
#endif


int devnull_read(int user_dst, uint64_t dst, int off, int n, char blocking, void *content)
{
    return 0;
}

int devnull_write(int user_src, uint64_t src, int off, int n, void *content)
{
    return 0;
}

int devzero_read(int user_dst, uint64_t dst, int off, int n, char blocking, void *content)
{
    char zeros[64] = { 0 };
    int target = n;

    while(n > 0) {
        int len = MIN(64, n);
        if (either_copyout(user_dst, dst, zeros, len) < 0)
            break;
        dst += len;
        n -= len;
    }
    return target;
}

void procfs_init_state(struct procfs_state *st)
{
    st->ksize = st->usize = 0;
    st->koff = st->uoff = 0;
}
