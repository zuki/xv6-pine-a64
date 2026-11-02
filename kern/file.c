/* File descriptors */

#include <types.h>
#include <fs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <file.h>
#include <console.h>
#include <vm.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <proc.h>

struct devsw devsw[NDEV];
struct {
    struct spinlock lock;
    struct file file[NOFILE];
} ftable;

/* Optional since BSS is zero-initialized. */
void fileinit(void)
{
    initlock(&ftable.lock, "file");
}

/* Allocate a file structure. */
struct file *filealloc(void)
{
    struct file *f;

    acquire(&ftable.lock);
    trace("acq ftable");
    for (f = ftable.file; f < ftable.file + NOFILE; f++) {
        if (f->ref == 0) {
            f->ref = 1;
            release(&ftable.lock);
            trace("rel ftable");
            return f;
        }
    }
    release(&ftable.lock);
    trace("rel ftable");
    return 0;
}

/* Increment ref count for file f. */
struct file *filedup(struct file *f)
{
    acquire(&ftable.lock);
    trace("acq ftable");
    if (f->ref < 1)
        panic("filedup");
    f->ref++;
    release(&ftable.lock);
    trace("rel ftable");
    return f;
}

/* Close file f. (Decrement ref count, close when reaches 0.) */
void fileclose(struct file *f)
{
    struct file ff;

    acquire(&ftable.lock);
    trace("acq ftable");
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0) {
        release(&ftable.lock);
        trace("rel ftable");
        return;
    }
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release(&ftable.lock);
    trace("rel ftable");

    if (ff.type == FD_PIPE)
        pipeclose(ff.pipe, ff.writable);
    else if (ff.type == FD_INODE || ff.type == FD_DEVICE) {
        iput(ff.ip);
    }
}

/* Get metadata about file f. */
int filestat(struct file *f, uint64_t stp)
{
    int error;

    if (f->type == FD_INODE || f->type == FD_DEVICE) {
        ilock(f->ip);
        error = stati(f->ip, stp);
        iunlock(f->ip);
        return error;
    }
    return -EFAULT;
}

/* Read from file f. */
ssize_t fileread(struct file *f, uint64_t addrp, ssize_t n, int user)
{
    ssize_t r;

    trace("f type: %d, readable: %d, major: %d, addrp: 0x%lx, n: %ld",
        f->type, f->readable, f->major, addrp, n);

    if (f->readable == 0) {
        trace("f is not readable");
        return -1;
    }

    if (f->type == FD_PIPE) {
        r = piperead(f->pipe, (char *)addrp, n);
    } else if (f->type == FD_DEVICE) {
        if (!devsw[f->major].read)
            return -EBADF;
        r = devsw[f->major].read(user, addrp, 0, n);
    } else if (f->type == FD_INODE) {
        ilock(f->ip);
        if ((r = readi(f->ip, user, (char *)addrp, f->off, n)) > 0)
            f->off += r;
        iunlock(f->ip);
    } else {
        error("invalid file type: %d", f->type);
        r = -EINVAL;
    }

    return r;
}

/* Write to file f. */
ssize_t filewrite(struct file *f, uint64_t addrp, ssize_t n, int user)
{
    ssize_t r = 0;

    trace("f: ino: %d, writeable: %d, type: %d, addrp: 0x%lx, size: %d",
        f->ip->inum, f->writable, f->type, addrp, n);

    if (f->writable == 0)
        return -1;

    if (f->type == FD_PIPE) {
        r = pipewrite(f->pipe, (char *)addrp, n);
    } else if (f->type == FD_DEVICE) {
        if (!devsw[f->major].write)
            return -EINVAL;
        r = devsw[f->major].write(user, addrp, f->off, n);
    } else if (f->type == FD_INODE) {
        /*
         * Write a few blocks at a time to avoid exceeding
         * the maximum log transaction size, including
         * i-node, indirect block, allocation blocks,
         * and 2 blocks of slop for non-aligned writes.
         * This really belongs lower down, since writei()
         * might be writing a device like the console.
         */
        ssize_t max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * 512;
        ssize_t i = 0;
        while (i < n) {
            ssize_t n1 = n - i;
            if (n1 > max)
                n1 = max;

            ilock(f->ip);
            if ((r = writei(f->ip, user, (char *)addrp + i, f->off, n1)) > 0)
                f->off += r;
            iunlock(f->ip);

            if (r < 0)
                break;
            if (r != n1)
                panic("short filewrite: r=%lld, n1=%lld\n", r, n1);
            i += r;
        }
        r = (i == n) ? n : -EIO;
    } else {
        error("invalid file type: %d", f->type);
        r = -EINVAL;
    }

    return r;
}

off_t filelseek(struct file *file, off_t offset, int whence)
{
    switch(whence) {
        case SEEK_SET:
            if (offset < 0)
                goto bad;
            else
                file->off = offset;
            break;
        case SEEK_CUR:
            if (file->off + offset < 0)
                goto bad;
            else
                file->off += offset;
            break;
        case SEEK_END:
            if (file->ip->size + offset < 0)
                goto bad;
            else
                file->off = file->ip->size + offset;
            break;
        default:
            goto bad;
    }

    return file->off;

bad:
    trace("invalid offset %d", offset)
    return -EINVAL;
}

int filegetdents(struct file *f, uint64_t dirp, size_t size)
{
    ssize_t n;
    int namelen, reclen, tlen = 0;
    off_t offset = f->off;
    struct dirent de;
    struct dirent64 de64;

    while (1) {
        n = fileread(f, (uint64_t)&de, DESIZE, 0);
        if (n == 0) {
            trace("read 0");
            return tlen ? tlen : 0;
        }
        if (n < 0 || n != DESIZE) {
            trace("read invalid n=%ld, tlen=%ld", n, tlen);
            return tlen ? tlen : -EIO;
        }

        if (de.inum == 0)
            continue;

        namelen = MIN(strlen(de.name), DIRSIZ) + 1;
        reclen = (size_t)(&((struct dirent64*)0)->d_name);
        reclen = reclen + namelen;
        reclen = (reclen + 0x7) & ~0x7;

        // sizeまで詰めたらreturn
        if ((tlen + reclen) > size) {
            trace("break; tlen: %d, reclen: %d, size: %d", tlen, reclen, size);
            break;
        }
        de64.d_ino = de.inum;
        de64.d_off = offset;
        de64.d_reclen = reclen;
        de64.d_type = IFTODT(f->ip->mode);
        memmove(de64.d_name, de.name, namelen);
        trace("de64: ino: %d, off: %d, reclen: %d, type: %d, name: %s",
            de64.d_ino, de64.d_off, de64.d_reclen, de64.d_type, de64.d_name);
        copyout(thisproc()->pgdir, (char *)dirp + tlen, (char *)&de64, reclen);
        tlen += reclen;
        offset = f->off;
    }
    return tlen;
}

int filewriteback(struct file *file, off_t offset, uint64_t addr)
{
    int r;
    struct timespec ts;
    struct inode *ip = file->ip;

    size_t n = offset + PGSIZE > ip->size ? ip->size - offset : PGSIZE;

    // ip->sizeもoffsetも0の場合があり、この場合は1ページ書き戻す
    if (n == 0) n = PGSIZE;

    trace("ip size=%d, offset=%d, n=%d", ip->size, offset, n);
    trace("addr[0]='%c', addr[1]='%c'", ((char *)addr)[0], ((char *)addr)[2]);

    ilock(ip);
    r = writei(ip, 0, (char *)addr, offset, n);
    //clock_gettime(CLOCK_REALTIME, &ts);
    //memmove(&ip->mtime, &ts, sizeof(struct timespec));
    //memmove(&ip->atime, &ts, sizeof(struct timespec));
    iunlock(ip);

    return r == n ? r : -EIO;
}

