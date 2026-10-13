//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <types.h>
#include <mmu.h>
#include <sched.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <string.h>
#include <console.h>
#include <fs.h>
#include <file.h>
#include <pipe.h>
#include <mm.h>
#include <vm.h>
//#include <filedesc.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <syscall.h>
//#include <net/sockio.h>
//#include <linux/ioctl.h>
#include <linux/stat.h>
#ifdef CONFIG_FAT
#include <ff.h>
#endif

/*
 * Fetch the nth word-sized system call argument as a file descriptor
 * and return both the descriptor and the corresponding struct file.
 */
static int argfd(int fd, struct file **pf)
{
    struct file *f;

    if (fd < 0 || fd >= NOFILE)
        return -EINVAL;
    if ((f = myproc()->ofile[fd]) == 0)
        return -EBADF;

    if (pf) *pf = f;

    return 0;
}

// AT_FDCWDをチェックする
static int check_fdcwd(const char *path, int fd)
{
    struct task_struct *p = myproc();

    if (*path != '/' && fd != AT_FDCWD) {
        if (p->ofile[fd] == 0) {
            error("pid[%d] path: %s (ofile[&d]) is not open)", p->pid, path, fd);
            return -EBADF;
        }

        if (p->ofile[fd]->ip->type != T_DIR) {
            error("pid[%d] path: %s fd->type %d is not directory)", p->pid, path, p->ofile[fd]->ip->type);
            return -ENOTDIR;
        }
    }
    return 0;
}

static int dupfd(int fd, int from)
{
    struct file *f = myproc()->ofile[fd];
    if (!f) return -EBADF;
    filedup(f);
    return fdalloc(f, from);
}

/* int dup(int oldfd); */
int sys_dup(int fd)
{
    return dupfd(fd, 0);
}

/* int dup3(int oldfd, int newfd, int flags); */
int sys_dup3(int fd1, int fd2, int flags)
{
    struct file *f;
    struct task_struct *p = myproc();

    if (flags & ~O_CLOEXEC) return -EINVAL;
    if (fd1 == fd2) return -EINVAL;
    if (fd1 < 0 || fd1 >= NOFILE) return -EBADF;
    if (fd2 < 0 || fd2 >= NOFILE) return -EBADF;

    f = p->ofile[fd1];
    if (p->ofile[fd2])
        fileclose(p->ofile[fd2]);
    filedup(f);

#if 0
    if (flags & O_CLOEXEC)
        bit_add(p->fdflag, fd2);
#endif
    p->ofile[fd2] = f;
    return fd2;
}

/* ssize_t read(int fd, void *buf, size_t count); */
ssize_t sys_read(int fd, uint64_t p /* user va */, size_t count)
{
    struct file *f;
    int error;

    if ((error = argfd(fd, &f)) < 0)
        return error;
    return fileread(f, p, count);
}

/*  ssize_t write(int fd, const void *buf, size_t count);  */
ssize_t sys_write(int fd, uint64_t p /* user va */, size_t count)
{
    struct file *f;
    int error;

    if ((error = argfd(fd, &f)) < 0)
        return error;
    return filewrite(f, p, count);
}

struct iovec {
    void *iov_base;             /* Starting address. */
    size_t iov_len;             /* Number of bytes to transfer. */
};

// ssize_t readv(int d, const struct iovec *iov, int iovcnt);
ssize_t sys_readv(int fd, uint64_t iovp, int iovcnt)
{
    struct file *f;
    struct iovec iov;
    struct mm_struct *mm = myproc()->mm;
    int error;
    ssize_t bytes;

    if ((error =argfd(fd, &f)) < 0)
        return error;

    ssize_t tot = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (copyin(mm, (char *)&iov, iovp, sizeof(struct iovec)) < 0)
            return -EIO;
        bytes = fileread(f, (uint64_t)iov.iov_base, iov.iov_len);
        if (bytes < 0) {
            error("bytes: %lld", bytes);
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
ssize_t sys_writev(int fd, uint64_t iovp, int iovcnt)
{
    struct file *f;
    struct iovec iov;
    struct mm_struct *mm = myproc()->mm;
    int error;

    if ((error =argfd(fd, &f)) < 0)
        return error;

    ssize_t tot = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (copyin(mm, (char *)&iov, iovp, sizeof(struct iovec)) < 0)
            return -EIO;
        tot += filewrite(f, (uint64_t)iov.iov_base, iov.iov_len);
        iovp += sizeof(iov);
    }
    return tot;
}

/*  off_t lseek(int fd, off_t offset, int whence); */
off_t sys_lseek(int fd, off_t offset, int whence)
{
    struct file *f;
    int error;

    if ((error = argfd(fd, &f)) < 0)
        return error;
    return filelseek(f, offset, whence);
}

/* int close(int fd); */
int sys_close(int fd)
{
    struct file *f;
    int error;

    if ((error = argfd(fd, &f)) < 0) {
        return error;
    }
    myproc()->ofile[fd] = 0;
    fileclose(f);
    return 0;
}

/* int fstat(int fd, struct stat *sb); */
int sys_fstat(int fd, uint64_t st /* user va */)
{
    struct file *f;
    int error;

    if ((error = argfd(fd, &f)) < 0)
        return error;

    return filestat(f, st);
}

#if 0
/* int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags); */
int  sys_fstatat(void)
{
    int dirfd, flags;
    char *path;
    char linkpath[512];
    struct stat *st, sst;
    struct vnode *vnode, *cwd = NULL;
    long error;
    memset(linkpath, 0, 512);

    if ((error = argint(0, &dirfd)) < 0)
        return error;
    if ((error = argstr(1, &path)) < 0)
        return error;
    if ((error = argptr(2, (void *)&st, sizeof(*st))) < 0)
        return error;
    if ((error = argint(3, &flags)) < 0)
        return error;
    trace("dirfd: %d, path: %s, flags: 0x%x", dirfd, path, flags);

#ifdef CONFIG_FAT
    if (is_fatfs(path)) {
        trace("cal fat_stat: %s", path);
        return fat_stat(path, st);
    }
#endif

    if (flags != 0 && (flags & ~AT_SYMLINK_NOFOLLOW) != 0) {
        warn("unimplemented flags=0x%x", flags);
        return -EINVAL;
    }

    if ((error = check_fdcwd(path, dirfd, &cwd)) < 0) {
        error("failed check_fdcwd: path: %s, dirfd: %d", path, dirfd);
        return error;
    }

    trace("lookup cwd: %d, path: %s", cwd->ino, path);
    if ((error = vfs_lookup(cwd, path, VLOOKUP_NORMAL, myproc()->uid, &vnode)) < 0) {
        error("vfs_lookup error: cwd: %d, path: %s", cwd->ino, path);
        return error;
    }

    trace("(1) path: %s, vnode: %d", path, vnode->ino);

    if (S_ISLNK(vnode->mode) && !(flags & AT_SYMLINK_NOFOLLOW)) {
        const char *name = path_last_component((const char *)path);
        if ((error = vfs_readlink(cwd, name, linkpath, 512, myproc()->uid)) < 0) {
            error("readlink: vnode: %d, name: %s", vnode->ino, name);
            vfs_release_vnode(cwd);
            return error;
        }
        linkpath[strlen(linkpath)] = '\0';
        trace("name: %s, linkpath: %s", name, linkpath);
        flags &= ~AT_SYMLINK_NOFOLLOW;
        vfs_release_vnode(vnode);
        if ((error = vfs_lookup(cwd, linkpath, VLOOKUP_NORMAL, myproc()->uid, &vnode)) < 0) {
            error("vfs_lookup error (%d): vnode: %d, linkpath: %s", error, vnode->ino, linkpath);
            vfs_release_vnode(cwd);
            return error;
        }
        trace("(2) linkvnode: %d", vnode->ino);
    }

    trace("(3) vnode: %d", vnode->ino);
    //sst.st_dev = vnode->mp->dev;
    sst.st_ino = vnode->ino;
    sst.st_mode = vnode->mode;
    sst.st_nlink = vnode->nlink;
    sst.st_uid = vnode->uid;
    sst.st_gid = vnode->gid;
    sst.st_rdev = vnode->rdev;
    sst.st_size = vnode->size;
    enum fsname fsname = get_fsname(vnode);
    if (fsname == FSV6)
        sst.st_dev = ((struct v6_inode *)vnode->data)->type;
    else
        sst.st_dev = T_UNKNOWN;
    memmove(&sst.st_atime, &vnode->atime, sizeof(struct timespec));
    memmove(&sst.st_mtime, &vnode->mtime, sizeof(struct timespec));
    memmove(&sst.st_ctime, &vnode->ctime, sizeof(struct timespec));
    memmove(st, &sst, sizeof(struct stat));
    trace("st_dev: %d", sst.st_dev);
    vfs_release_vnode(vnode);

    trace("dirfd: %d, path: %s, ino: %d", dirfd, path, sst.st_ino);
    return 0;
}
#endif

/* int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);  */
int  sys_linkat(int ofd, uint64_t opathp /* user va*/,
    int nfd, uint64_t npathp /* user va*/, int flags)
{
    char opath[MAXPATH], npath[MAXPATH];
    struct inode *ip;
    int error;
    //uid_t uid = myproc()->uid;

    if (flags & ~AT_SYMLINK_FOLLOW)
        return -EINVAL;

    if (argstr(opathp, opath, MAXPATH) < 0)
        return -EIO;
    if (argstr(npathp, npath, MAXPATH) < 0)
        return -EIO;

    if ((error = check_fdcwd(opath, ofd)) < 0)
        return error;
    if ((error = check_fdcwd(npath, nfd)) < 0)
        return error;

    if (flags & AT_SYMLINK_FOLLOW) {
        if ((ip = namei(opath, ofd)) == 0)
            return -ENOENT;
        ilock(ip);
        if (ip->type == T_SYMLINK) {
            int n;
            if ((n = readi(ip, 0, (uint64_t)opath, 0, sizeof(MAXPATH) - 1)) < 0) {
                warn("could not follow symlink");
                iunlockput(ip);
                return -ENOENT;
            }
            opath[n] = 0;
        }
        iunlockput(ip);
    } else if (flags != 0) {
        error("flags 0x%x is not supported", flags);
        return -EINVAL;
    }

    return filelink(opath, ofd, npath, nfd);
}

//int symlinkat(const char *targetpath, int newdirfd, const char *newpath);
ssize_t sys_symlinkat(uint64_t tpathp/* user va*/, int nfd, uint64_t npathp /* user va*/)
{
    char tpath[MAXPATH], npath[MAXPATH];
    int error;

    if (argstr(tpathp, tpath, MAXPATH) < 0)
        return -EIO;
    if (argstr(npathp, npath, MAXPATH) < 0)
        return -EIO;

    if (strncmp(tpath, "", 1) == 0 || strncmp(npath, "", 1) == 0)
        return -ENOENT;

    if ((error = check_fdcwd(npath, nfd)) < 0)
        return error;

    return filesymlink(tpath, npath, nfd);
}

/* int unlinkat(int dirfd, const char *pathname, int flags); */
int  sys_unlinkat(int fd, uint64_t pathp /* user va*/, int flags)
{
    char path[MAXPATH];
    int error;

    if (argstr(pathp, path, MAXPATH) < 0)
        return -EIO;
    if ((error = check_fdcwd(path, fd)) < 0)
        return error;

    if (flags & ~AT_REMOVEDIR)
        return -EINVAL;

#ifdef CONFIG_FAT
    if (is_fatfs(path))
        return fat_unlink(path);
#endif

    return fileunlink(path, fd, flags);
}

/* ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz); */
ssize_t sys_readlinkat(int fd, uint64_t pathp/* user va*/, uint64_t bufp/* user va*/, size_t bufsize)
{
    char path[MAXPATH];
    int error;

    if (argstr(pathp, path, MAXPATH) < 0)
        return -EIO;

    if ((error = check_fdcwd(path, fd)) < 0)
        return error;

    return filereadlink(path, fd, (char *)bufp, bufsize/* myproc()->uid */);
}

/* int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath); */
int sys_renameat(int ofd, uint64_t opathp/* user va*/, int nfd, uint64_t npathp/* user va*/)
{
    char npath[MAXPATH], opath[MAXPATH];
    int error;

    if (argstr(opathp, opath, MAXPATH) < 0)
        return -EIO;
    if (argstr(npathp, npath, MAXPATH) < 0)
        return -EIO;

    if ((error = check_fdcwd(opath, ofd)) < 0)
        return error;
    if ((error = check_fdcwd(npath, nfd)) < 0)
        return error;

    return filerename(opath, ofd, npath, nfd);
}

/* int renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags); */
int sys_renameat2(int ofd, uint64_t opathp/* user va*/,
    int nfd, uint64_t npathp/* user va*/, int flags)
{
    // muslでは RENAME_NOREPLACEもRENAME_EXCHANGEも対応していない
    if (flags)
        return -EINVAL;

    return sys_renameat(ofd, opathp, nfd, npathp);
}

/* int openat(int dirfd, const char *pathname, int flags, mode_t mode); */
int sys_openat(int fd, uint64_t pathp/* user va*/, int flags, mode_t mode)
{
    char path[MAXPATH];
     int error;

    if (argstr(pathp, path, MAXPATH) < 0)
        return -EIO;

#if 0
    if (mode)
        mode = (mode & ~(myproc()->umask)) & 0777;
#endif

#ifdef CONFIG_FAT
    if (is_fatfs(path))
        return fat_open(path, flags, mode);
#endif

    if ((error = check_fdcwd(path, fd)) < 0)
        return error;

    return fileopen(path, fd, flags, mode);
}

// int mkdirat(int dirfd, const char *pathname, mode_t mode);
int sys_mkdirat(int fd, uint64_t pathp/* user va*/, mode_t mode)
{
    char path[MAXPATH];
    int error;

    if (argstr(pathp, path, MAXPATH) < 0)
        return -EIO;

#ifdef CONFIG_FAT
    if (is_fatfs(path))
        return fat_mkdir(path);
#endif

    if ((error = check_fdcwd(path, fd)) < 0)
        return error;
#if 0
    mode = (mode & ~(myproc()->umask)) & 0777;
#endif

    if ((error = fileopen(path, fd, O_CREAT, S_IFDIR | mode)) < 0)
        return error;
    fileclose(myproc()->ofile[error]);
    return 0;
}

/* int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev); */
int sys_mknodat(int fd, uint64_t pathp/* user va*/, mode_t mode, dev_t dev)
{
    char path[MAXPATH];
    int error;

    if (argstr(pathp, path, MAXPATH) < 0)
        return -EIO;
    if ((error = check_fdcwd(path, fd)) < 0)
        return error;

    return filemknod(path, mode, (device_t)dev);
}

/* int chdir(const char *path); */
int sys_chdir(uint64_t pathp/* user va*/)
{
    char path[MAXPATH];
    int error;

    if (argstr(pathp, path, MAXPATH) < 0)
        return -EIO;

#ifdef CONFIG_FAT
    if (is_fatfs(path))
        return fat_chdir(path);
#endif

    if ((error = check_fdcwd(path, AT_FDCWD)) < 0)
        return error;

    return filechdir(path);
}

// TODO: 次はここから
/*  int execve(const char *filename, char *const argv[], char *const envp[]); */
int sys_execve(uint64_t pathp/* user va*/, uint64_t argvp/* user va*/, uint64_t envpp/* user va*/)
{
    char path[MAXPATH], buf[MAXPATH];
    char *argv[MAXARG], *envp[MAXARG];
    uint64_t arg_p, env_p;
    int argc = 0, envc = 0, error;

    if (argstr(pathp, path, MAXPATH) < 0)
        return -EIO;

    memset(argv, 0, sizeof(argv));
    if (argvp != 0 && argvp != -1) {
        for (;; argc++) {
            if (argc >= MAXARG - 1) {
                error("too many arg: %d", argc);
                error = -E2BIG;
                goto bad;
            }
            if (argptr(argvp + argc * sizeof(uint64_t), (char *)&arg_p, sizeof(uint64_t)) < 0) {
                error("argptr  argc_p[%d]: %d", argc);
                error = -EFAULT;
                goto bad;
            }
            if (arg_p == 0) {
                argv[argc] = 0;
                break;
            }
            if ((error = argstr(arg_p, buf, MAXPATH)) < 0) {
                error("argstr argv[%d]", argc);
                error = -EIO;
                goto bad;
            }
            argv[argc] = kmalloc(error + 1);
            if (argv[argc] == 0) {
                error("out of memory for argv[%d]", argc);
                error = -ENOMEM;
                goto bad;
            }
            memcpy(argv[argc], buf, error + 1);
        }
    } else {
        argc = 0;
    }

    memset(envp, 0, sizeof(envp));
    if (envpp != 0 && envpp != -1) {
        for (;; envc++) {
            if (envc >= MAXARG - 1) {
                error("too many arg: %d", envc);
                error = -E2BIG;
                goto bad;
            }
            if (argptr(envpp + envc * sizeof(uint64_t), (char *)&env_p, sizeof(uint64_t)) < 0) {
                error("argptr envc_p[%d]: %d", envc);
                error = -EFAULT;
                goto bad;
            }
            if (env_p == 0) {
                envp[envc] = 0;
                break;
            }
            if ((error = argstr(env_p, buf, MAXPATH)) < 0) {
                error("etchstr envp[%d]", envc);
                error = -EIO;
                goto bad;
            }
            envp[envc] = kmalloc(error + 1);
            if (envp[envc] == 0) {
                error("out of memory for envp[%d]", envc);
                error = -ENOMEM;
                goto bad;
            }
            memcpy(envp[envc], buf, error + 1);
        }
    } else {
        envc = 0;
    }

    error = execve(path, argv, envp, argc, envc);
    for (int i = 0; i < argc; i++) {
        kmfree(argv[i]);
    }
    for (int i = 0; i < envc; i++) {
        kmfree(envp[i]);
    }

    return error;

bad:
    for (int i = 0; i < argc; i++) {
        kmfree(argv[i]);
    }
    for (int i = 0; i < envc; i++) {
        kmfree(envp[i]);
    }
    return error;
}

/* int pipe2(int pipefd[2], int flags); */
int sys_pipe2(uint64_t pipefdp/* user va*/, int flags)
{
    int fd0, fd1;
    uint64_t fdary;
    struct file *rf, *wf;
    struct task_struct *p = myproc();

    if (flags & ~PIPE2_FLAGS) {
        warn("invalid lags: 0x%x", flags);
        return -EINVAL;
    }

    if (pipealloc(&rf, &wf, flags) < 0)
        return -ENFILE;

    if ((fd0 = fdalloc(rf, 0)) < 0 || (fd1 < fdalloc(wf, 0)) < 0) {
        if (fd0 >= 0) {
            p->ofile[fd0] = 0;
        }
        fileclose(rf);
        fileclose(wf);
        return -EMFILE;
    }

    if ((copyout(p->mm, fdary, (char *)&fd0, sizeof(fd0)) < 0)
     || (copyout(p->mm, fdary + sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0)) {
        p->ofile[fd0] = 0;
        p->ofile[fd1] = 0;
#if 0
        bit_remove(p->fdflag, fd0);
        bit_remove(p->fdflag, fd1);
#endif
        fileclose(rf);
        fileclose(wf);
        return -EFAULT;
    }

#if 0
    if (flags & O_CLOEXEC) {
        bit_add(p->fdflag, fd0);
        bit_add(p->fdflag, fd1);
    }
#endif

    return 0;
}

// int ioctl(int fd, unsigned long request, ...);
int sys_ioctl(int fd, uint64_t req, uint64_t argp)
{
    return -ENOTTY;
#if 0
    struct file *f;
    uint64_t argp;
    int error;

    if ((error = argfd(fd, &f)) < 0)
        return error;

    if (f->type != FD_INODE && f->ip->type != T_DEVICE) {
        warn("bad ftype: %d, ip->type: %d", f->type, f->ip->type);
        return -ENOTTY;
    }

    return fileioctl(f, req, (void *)argp);
#endif
}

/* int fcntl(int fd, int cmd, ...); */
int sys_fcntl(int fd, int cmd, int args)
{
    struct file *f;
    int error;

    if ((error = argfd(fd, &f)) < 0)
        return error;

    switch (cmd) {
        case F_DUPFD:
            return dupfd(fd, args);

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
            return (f->flags & (FILE_STATUS_FLAGS | O_ACCMODE));

        case F_SETFL:
            f->flags = ((args & FILE_STATUS_FLAGS) | (f->flags & O_ACCMODE));
            return 0;
    }

    return -EINVAL;
}


// ssize_t getdents64(int fd, void *dirp, size_t count);
int sys_getdents64(int fd, uint64_t dirp, size_t count)
{
    struct file *f;
    int error;

    if ((error = argfd(fd, &f)) < 0)
        return error;

    if (f->ip->type == T_DEVICE)
        return 0;
    if (f->ip->type != T_DIR)
        return -ENOTDIR;

    return getdents64(f, dirp, count);
}

#if 0
/* オープンされたファイルディスクリプター fd により参照されるファイルの許可を変更する */
// int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);
int  sys_fchmodat()
{
    int dirfd, flags;
    char *path;
    mode_t mode;
    long error;
    struct vnode *vnode;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
     || argint(2, (int *)&mode) < 0 || argint(3, &flags) < 0)
        return -EINVAL;

    if ((error = check_fdcwd(path, dirfd, &vnode)) < 0) return error;

    if (flags & AT_SYMLINK_NOFOLLOW)
        return -ENOTSUP;

    return vfs_chmod(vnode, path, mode, myproc()->uid);
}

/* pathname で指定されたファイルの所有権を変更する*/
// int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);
int  sys_fchownat(void)
{
    int dirfd, flags;
    char *path;
    uid_t owner;
    gid_t group;
    long error;
    struct vnode *vnode;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
     || argint(2, (int *)&owner) < 0 || argint(3, (int *)&group) < 0
     || argint(4, &flags) < 0)
        return -EINVAL;

    trace("dirfd=%d, path=%s, uid=%d, gid=%d, flags=%d\n", dirfd, path, owner, group, flags);

    if ((error = check_fdcwd(path, dirfd, &vnode)) < 0) return error;

    if (flags)
        return -ENOTSUP;

    return vfs_chown(vnode, path, owner, group, myproc()->uid);
}

/* オープンされたファイルディスクリプター fd により参照されるファイルの所有権を変更する */
// int fchown(int fd, uid_t owner, gid_t group);
int  sys_fchown(void)
{
    //int fd;
    uid_t owner;
    gid_t group;
    struct file *f;

    if (argfd(0, 0, &f) < 0 || argint(1, (int *)&owner) < 0
     || argint(2, (int *)&group) < 0)
        return -EINVAL;

    return vfs_fchown(f, owner, group);
}

/* 呼び出し元プロセスがファイル pathname にアクセスできるか否かチェックする */
// int faccessat(int dirfd, const char *pathname, int mode, int flags); flags = 0
int  sys_faccessat(void)
{
    int dirfd, mode;
    char *path;
    long error;
    struct vnode *vnode;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
     || argint(2, &mode) < 0)
        return -EINVAL;

    if ((error = check_fdcwd(path, dirfd, &vnode)) < 0) return error;

    return vfs_access(vnode, path, mode, myproc()->uid, 0);
}

/* 呼び出し元プロセスがファイル pathname にアクセスできるか否かチェックする */
// int faccessat(int dirfd, const char *pathname, int mode, int flags);
int  sys_faccessat2(void)
{
    int dirfd, mode, flags;
    char *path;
    long error;
    struct vnode *vnode;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
     || argint(2, &mode) < 0  || argint(3, &flags) < 0)
        return -EINVAL;

    if ((error = check_fdcwd(path, dirfd, &vnode)) < 0) return error;

    return vfs_access(vnode, path, mode, myproc()->uid, flags);
}
#endif
