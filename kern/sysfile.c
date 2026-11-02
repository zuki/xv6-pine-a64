#include <types.h>
#include <linux/fcntl.h>
#include <syscall.h>
#include <types.h>
#include <mmu.h>
#include <proc.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <string.h>
#include <console.h>
#include <fs.h>
#include <file.h>
#include <vm.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/termios.h>

extern int execve(const char *, char *const, char *const);

struct iovec {
    void *iov_base;             /* Starting address. */
    size_t iov_len;             /* Number of bytes to transfer. */
};

/*
 * Fetch the nth word-sized system call argument as a file descriptor
 * and return both the descriptor and the corresponding struct file.
 */
long argfd(int n, int *pfd, struct file **pf)
{
    int fd;
    struct file *f;

    if (argint(n, &fd) < 0)
        return -1;
    if (fd < 0 || fd >= NOFILE || (f = thisproc()->ofile[fd]) == 0)
        return -1;
    if (pfd)
        *pfd = fd;
    if (pf)
        *pf = f;
    return 0;
}

/*
 * Allocate a file descriptor for the given file.
 * Takes over file reference from caller on success.
 */
static int fdalloc(struct file *f, int start)
{
    struct proc *curproc = thisproc();

    for (int fd = start; fd < NOFILE; fd++) {
        if (curproc->ofile[fd] == 0) {
            curproc->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

long sys_dup()
{
    struct file *f;
    int fd;

    if (argfd(0, 0, &f) < 0)
        return -1;
    if ((fd = fdalloc(f, 0)) < 0)
        return -1;
    trace("fd %d", fd);
    filedup(f);
    return fd;
}

// ssize_t read(int fd, void *buf, size_t count);
ssize_t sys_read()
{
    struct file *f;
    ssize_t n;
    uint64_t bufp;
    int fd;

    if (argfd(0, &fd, &f) < 0 || argu64(1, &bufp) < 0 || argu64(2, (uint64_t *)&n) < 0)
        return -1;

#if 0
    trace("fd: %d, inum: %d, bufp: 0x%lx, n: %ld", fd, f->ip->inum, bufp, n);
    ssize_t bytes = fileread(f, bufp, n, 1);
    char kbuf[bytes+1];
    copyin(thisproc()->pgdir, kbuf, bufp, bytes);
    debug("read: %s, %ld bytes", kbuf, bytes);
    return bytes;
#endif

    return fileread(f, bufp, n, 1);
}

//  ssize_t write(int fd, const void *buf, size_t count);
ssize_t sys_write()
{
    struct file *f;
    ssize_t n;
    uint64_t bufp;

    if (argfd(0, 0, &f) < 0 || argu64(1, &bufp) < 0 || argu64(2, (uint64_t *)&n) < 0)
        return -1;

    trace("ip: %d, bufp: 0x%lx, n: %ld", f->ip->inum, bufp, n);
    return filewrite(f, bufp, n, 1);
}

// ssize_t readv(int d, const struct iovec *iov, int iovcnt);
ssize_t sys_readv()
{
    int fd, iovcnt;
    uint64_t iovp;
    struct file *f;
    struct iovec iov;
    ssize_t bytes;

    if (argfd(0, &fd, &f) || argu64(1, &iovp) < 0
     || argint(2, &iovcnt) < 0)
        return -EINVAL;

    ssize_t tot = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (copyin(thisproc()->pgdir, (char *)&iov, iovp, sizeof(struct iovec)) < 0)
            return -EIO;
        bytes = fileread(f, (uint64_t)iov.iov_base, iov.iov_len, 0);
        if (bytes < 0) {
            error("[%d] bytes: %ld", i, bytes);
            return bytes;
        }
        tot += bytes;
        if (bytes == 0)
            break;
        iovp += sizeof(struct iovec);
    }
    return tot;
}

/* ssize_t writev(int fd, const struct iovec *iov, int iovcnt); */
ssize_t sys_writev()
{
    struct file *f;
    int fd, iovcnt;
    uint64_t iovp;
    struct iovec iov;

    if (argfd(0, &fd, &f) < 0 ||
        argu64(1, &iovp) < 0 ||
        argint(2, &iovcnt) < 0) {
        return -1;
    }
    trace("fd %d, inum: %d, iovp: 0x%lx, iovcnt: %d", fd, f->ip->inum, iovp, iovcnt);

    size_t tot = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (copyin(thisproc()->pgdir, (char *)&iov, iovp, sizeof(struct iovec)) < 0) {
            error("copyin iov[%d] failed", i);
            return -EIO;
        }
        tot += filewrite(f, (uint64_t)iov.iov_base, iov.iov_len, 0);
        iovp += sizeof(iov);
    }
    return tot;
}

long sys_close()
{
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0)
        return -1;
    thisproc()->ofile[fd] = 0;
    fileclose(f);
    return 0;
}

// int fstat(int fd, struct stat *buf);
long sys_fstat()
{
    int fd;
    struct file *f;
    uint64_t stp;

    if (argfd(0, &fd, &f) < 0 || argu64(1, &stp) < 0)
        return -1;
    trace("fd %d", fd);
    return filestat(f, stp);
}

// int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);
long sys_fstatat()
{
    int dirfd, flags;
    char *path;
    uint64_t stp;
    int error;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 ||
        argu64(2, &stp) < 0 || argint(3, &flags) < 0)
        return -1;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -1;
    }

    // FIXME: AT_SYMLINK_NOFOLLOW の実装
#if 0
    if (flags != 0) {
        warn("flags: 0x%x unimplemented", flags);
    }
#endif

    struct inode *ip;
    if ((ip = namei(path)) == 0) {
        return -1;
    }
    trace("path: %s, ip: %d, dev: %d", path, ip->inum, ip->dev);

    ilock(ip);
    error = stati(ip, stp);
    iunlockput(ip);

    return error;
}

/* Create the path new as a link to the same inode as old. */
long sys_link()
{
    char name[DIRSIZ], *new, *old;
    struct inode *dp, *ip;

    if (argstr(0, &old) < 0 || argstr(1, &new) < 0)
        return -1;

    if ((ip = namei(old)) == 0) {
        return -1;
    }

    ilock(ip);
    if (ip->type == T_DIR) {
        iunlockput(ip);
        return -1;
    }

    ip->nlink++;
    iupdate(ip);
    iunlock(ip);

    if ((dp = nameiparent(new, name)) == 0)
        goto bad;
    ilock(dp);
    if (dp->dev != ip->dev || dirlink(dp, name, ip->inum, ip->type) < 0) {
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
    return -1;
}

/* Is the directory dp empty except for "." and ".." ? */
static int isdirempty(struct inode *dp)
{
    struct dirent de;

    for (ssize_t off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (char *)&de, off, sizeof(de)) != sizeof(de))
            panic("isdirempty: readi");
        if (de.inum != 0)
            return 0;
    }
    return 1;
}

long sys_unlinkat()
{
    struct inode *ip, *dp;
    struct dirent de;
    char name[DIRSIZ], *path;
    size_t off;
    int dirfd;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0)  // flagsは当面無視
        return -1;

    if (dirfd != AT_FDCWD)
        return -1;

    if ((dp = nameiparent(path, name)) == 0) {
        return -1;
    }

    ilock(dp);

    /* Cannot unlink "." or "..". */
    if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
        goto bad;

    if ((ip = dirlookup(dp, name, &off)) == 0)
        goto bad;
    ilock(ip);

    if (ip->nlink < 1)
        panic("unlink: nlink < 1");
    if (ip->type == T_DIR && !isdirempty(ip)) {
        iunlockput(ip);
        goto bad;
    }

    memset(&de, 0, sizeof(de));
    if (writei(dp, 0, (char *)&de, off, sizeof(de)) != sizeof(de))
        panic("unlink: writei");
    if (ip->type == T_DIR) {
        dp->nlink--;
        iupdate(dp);
    }
    iunlockput(dp);

    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);

    return 0;

  bad:
    iunlockput(dp);
    return -1;
}

static struct inode *create(char *path, short type, short major, short minor, mode_t mode)
{
    struct inode *ip, *dp;
    char name[DIRSIZ];

    if ((dp = nameiparent(path, name)) == 0)
        return 0;
    ilock(dp);

    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iunlockput(dp);
        ilock(ip);
        if (type == T_FILE && ip->type == T_FILE)
            return ip;
        iunlockput(ip);
        return 0;
    }

    if ((ip = ialloc(dp->dev, type)) == 0)
        panic("create: ialloc");

    ilock(ip);
    ip->devno = makedev(major, minor);
    ip->nlink = 1;
    ip->mode = mode;
    ip->type = type;
    iupdate(ip);

    if (type == T_DIR) {        // Create . and .. entries.
        dp->nlink++;            // for ".."
        iupdate(dp);
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (dirlink(ip, ".", ip->inum, ip->type) < 0
            || dirlink(ip, "..", dp->inum, dp->type) < 0)
            panic("create dots");
    }

    if (dirlink(dp, name, ip->inum, ip->type) < 0)
        panic("create: dirlink");

    iunlockput(dp);

    return ip;
}

long sys_openat()
{
    char *path;
    int dirfd, fd, flags;
    mode_t mode;
    struct file *f;
    struct inode *ip;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
        || argint(2, &flags) < 0 || argint(3, (int *)&mode) < 0)
        return -1;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -1;
    }
    if ((flags & O_LARGEFILE) == 0) {
        warn("expect O_LARGEFILE in open flags");
        return -1;
    }
    trace("dirfd %d, path '%s', flag 0x%x", dirfd, path, flags);

    if (flags & O_CREAT) {
        trace("create");
        // FIXME: acl mode are ignored.
        ip = create(path, T_FILE, 0, 0, mode | S_IFREG);
        if (ip == 0) {
            return -1;
        }
    } else {
        if ((ip = namei(path)) == 0 || ip->type == 0) {
            return -1;
        }
        trace("ip->inum: %d, type: %d", ip->inum, ip->type);
        ilock(ip);
        // FIXME: このflagsのチェックの意味を考える
        if (ip->type == T_DIR && (flags & (O_RDONLY | O_LARGEFILE) != (O_RDONLY | O_LARGEFILE))) {
            iunlockput(ip);
            return -1;
        }
    }

    if ((f = filealloc()) == 0 || (fd = fdalloc(f, 0)) < 0) {
        if (f)
            fileclose(f);
        iunlockput(ip);
        return -1;
    }

    trace("path: %s, ip: ino: %d, type: %d, devno: 0x%x", path, ip->inum, ip->type, ip->devno);

    if (ip->type == T_DEVICE) {
        f->type  = FD_DEVICE;
        f->major = major(ip->devno);
        f->off   = 0;
    } else {
        f->type = FD_INODE;
        f->off = 0;
    }

    f->ip = ip;
    f->readable = !(flags & O_WRONLY);
    f->writable = (flags & O_WRONLY) || (flags & O_RDWR);

    iunlock(ip);
    trace("path: %s, fd: %d", path, fd);
    return fd;
}

long sys_mkdirat()
{
    int dirfd, mode;
    char *path;
    struct inode *ip;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
        || argint(2, &mode) < 0)
        return -1;
    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -1;
    }
    if (mode != 0) {
        warn("mode unimplemented");
        return -1;
    }
    trace("path '%s', mode 0x%x", path, mode);

    if ((ip = create(path, T_DIR, 0, 0, mode | S_IFDIR)) == 0) {
        return -1;
    }
    iunlockput(ip);

    return 0;
}

/* int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev); */
long sys_mknodat()
{
    struct inode *ip;
    char *path;
    int dirfd;
    mode_t mode;
    dev_t dev;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
        || argint(2, (int *)&mode) < 0 || argu64(3, &dev))
        return -1;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -1;
    }
    trace("path '%s', mode: 0x%x, dev: 0x%lx", path, mode, dev);

    if ((ip = create(path, T_DEVICE, major(dev), minor(dev), mode)) == 0) {
        return -1;
    }
    iunlockput(ip);
    return 0;
}

long sys_chdir()
{
    char *path;
    struct inode *ip;
    struct proc *curproc = thisproc();

    if (argstr(0, &path) < 0 || (ip = namei(path)) == 0) {
        return -1;
    }
    ilock(ip);
    if (ip->type != T_DIR) {
        iunlockput(ip);
        return -1;
    }
    iunlock(ip);
    iput(curproc->cwd);
    curproc->cwd = ip;
    return 0;
}

long sys_execve()
{
    char *p;
    void *argv, *envp;
    if (argstr(0, &p) < 0 || argu64(1, (uint64_t *) & argv) < 0
        || argu64(2, (uint64_t *) & envp) < 0)
        return -1;
    return execve(p, argv, envp);
}

long sys_pipe2()
{
    int *fd, flag;
    struct file *rf, *wf;
    int fd0, fd1;

    if (argint(1, &flag) < 0
        || argptr(0, (void *)&fd, 2 * sizeof(fd[0])) < 0)
        return -1;
    trace("flag 0x%x", flag);
    if (flag) {
        warn("pipe with flag unimplemented");
        return -1;
    }
    if (pipealloc(&rf, &wf) < 0)
        return -1;
    fd0 = -1;
    if ((fd0 = fdalloc(rf, 0)) < 0 || (fd1 = fdalloc(wf, fd0)) < 0) {
        if (fd0 >= 0)
            thisproc()->ofile[fd0] = 0;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }
    fd[0] = fd0;
    fd[1] = fd1;
    return 0;
}

//  off_t lseek(int fd, off_t offset, int whence);
long sys_lseek(void)
{
    int fd;
    off_t offset;
    int whence;
    struct file *file;

    if (argfd(0, &fd, &file) < 0
    || argu64(1, (uint64_t *)&offset) < 0
    || argint(2, &whence) < 0)
        return -EINVAL;

    return filelseek(file, offset, whence);
}


// ssize_t getdents64(int fd, void *dirp, size_t count);
long sys_getdents64(void)
{
    int fd;
    uint64_t dirp;
    size_t size;
    struct file *file;
    long error;

    if ((error = argfd(0, &fd, &file)) < 0) return error;
    if ((error = argu64(1, &dirp)) < 0) return error;
    if ((error = argu64(2, &size)) < 0) return error;

    trace("fd: %d, ino: %d, readable: %d", fd, file->ip->inum, file->readable);
    return filegetdents(file, dirp, size);
}

// int ioctl(int fd, unsigned long request, ...);
// FIXME: Hack TIOCGWINSZ(get window size)
long sys_ioctl() {
    int fd;
    uint64_t req, argp;
    struct file *f;

    if (argfd(0, &fd, &f) < 0 || argu64(1, &req) < 0
    || argu64(2, &argp) < 0)
        return -EINVAL;

    trace("fd: %d, inum: %d, req: 0x%lx, argp: 0x%lx",
        fd, f->ip->inum, req, argp);

    switch(req) {
        case TIOCGWINSZ:
            if (argp == 0) return -EINVAL;
            ((struct winsize *)argp)->ws_row = 24;
            ((struct winsize *)argp)->ws_col = 80;
            return 0;
        case TIOCSWINSZ:
            // Windowサイズ設定: 当面何もしない
            return 0;
        default:
            return -ENOTTY;
    }
}

/* int fcntl(int fd, int cmd, ...); */
long sys_fcntl(void)
{
    struct file *file;
    //struct proc *p = thisproc();
    int fd, fd1, cmd, args;
    long error;

    if (argfd(0, &fd, &file) < 0 || argint(1, &cmd) < 0
    || argint(2, &args) < 0)
        return -EINVAL;

    trace("fd: %d, cmd: 0x%x", fd, cmd);

    switch (cmd) {
        case F_DUPFD:
            if ((fd1 = fdalloc(file, args)) < 0)
                return fd1;
            filedup(file);
            return fd1;
        case F_GETFD:
            //return bit_test(p->fdflag, fd) ? FD_CLOEXEC : 0;
            return 0;
        case F_SETFD:
#if 0
            if (args & FD_CLOEXEC)
                bit_add(p->fdflag, fd);
            else
                bit_remove(p->fdflag, fd);
#endif
            return 0;

        case F_GETFL:
            return (file->flags & (FILE_STATUS_FLAGS | O_ACCMODE));

        case F_SETFL:
            file->flags = ((args & FILE_STATUS_FLAGS) | (file->flags & O_ACCMODE));
            return 0;
    }

    return -EINVAL;
}
