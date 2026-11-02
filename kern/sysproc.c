#include <types.h>
#include <syscall.h>
#include <proc.h>
#include <trap.h>
#include <console.h>
#include <mmap.h>
#include <vm.h>
#include <linux/syscall.h>
#include <linux/mman.h>
#include <linux/errno.h>

long sys_yield()
{
    yield();
    //wfi();
    return 0;
}

size_t sys_brk()
{
    struct proc *p = thisproc();
    size_t sz, newsz, oldsz = p->sz;

    if (argu64(0, &newsz) < 0)
        return oldsz;

    trace("name %s: 0x%llx to 0x%llx", p->name, oldsz, newsz);

    if (newsz == 0)
        return oldsz;

    if (newsz < oldsz) {
        p->sz = uvm_dealloc(p->pgdir, p->base, oldsz, newsz);
    } else {
        sz = uvm_alloc(p->pgdir, p->base, p->stksz, oldsz, newsz);
        if (sz == 0)
            return oldsz;
        p->sz = sz;
    }
    return p->sz;
}

// void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
void *sys_mmap()
{
    uint64_t addrp;
    size_t length, offset;
    int prot, flags, fd;
    struct file *f;

    if (argu64(0, &addrp) < 0 ||
        argu64(1, &length) < 0 ||
        argint(2, &prot) < 0 ||
        argint(3, &flags) < 0 || argint(4, &fd) < 0 || argu64(5, &offset) < 0)
        return (void *)-1;

    trace("addrp: 0x%lx, length: 0x%lx, proc: 0x%x, flags: 0x%x, fd: %d, offset: 0x%x",
        addrp, length, prot, flags, fd, offset);

    if (flags & MAP_ANONYMOUS) {
        if (fd != -1) return (void *)-EINVAL;
        f = NULL;
    } else {
        if (fd < 0 || fd >= NOFILE)
            return (void *)-EBADF;
        if ((f = thisproc()->ofile[fd]) == NULL)
            return (void *)-EBADF;
    }

    if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0) {
        warn("invalid flags: 0x%x", flags);
        return (void *)-EINVAL;
    }

    if ((ssize_t)length <= 0 || (ssize_t)offset < 0) {
        warn("invalid length: %lld or offset: %lld", length, offset);
        return (void *)-EINVAL;
    }

    // MAP_FIXEDの場合、addrが指定されていなければならない
    if ((flags & MAP_FIXED) && addrp == 0) {
        warn("MAP_FIXED and addr is NULL");
        return (void *)-EINVAL;
    }

    // バックにあるファイルはreadableでなければならない
    if (!(flags & MAP_ANONYMOUS) && !FILE_READABLE(f)) {
        warn("file is not readable");
        return (void *)-EACCES;
    }

    // MAP_SHAREかつPROT_WRITEの場合はバックにあるファイルがwritableでなければならない
    if (!(flags & MAP_ANONYMOUS) && (flags & MAP_SHARED)
     && (prot & PROT_WRITE) && !FILE_WRITABLE(f)) {
        warn("file is not writable");
        return (void *)-EACCES;
    }

    trace("addr: 0x%llx, length: 0x%x, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%x",
        addr, length, prot, flags, f ? f->vnode->ino : -1, offset);
    return mmap((void *)addrp, length, prot, flags, f, offset);
}

// int munmap(void *addr, size_t length);
long sys_munmap(void)
{
    uint64_t addr;
    size_t length;

    if (argu64(0, &addr) < 0 || argu64(1, &length) < 0)
        return -EINVAL;

    trace("addr: 0x%llx, length: 0x%llx", addr, length);

    return munmap((void *)addr, length);
}

// void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, void *new_address);
void *sys_mremap(void)
{
    uint64_t old_addr, new_addr;
    size_t old_size, new_size;
    int flags;

    if (argu64(0, &old_addr) < 0 || argu64(1, &old_size) < 0
     || argu64(2, &new_size) < 0 || argint(3, &flags) < 0
     || argu64(4, &new_addr) < 0)
        return (void *)-EINVAL;

    return mremap((void *)old_addr, old_size, new_size, flags, (void *)new_addr);
}

// int mprotect(void *addr, size_t len, int prot);
long sys_mprotect(void)
{
    uint64_t addr;
    size_t length;
    int prot;

    if (argu64(0, &addr) < 0 || argu64(1, &length) < 0
     || argint(2, &prot) < 0)
        return -EINVAL;

    return mprotect((void *)addr, length, prot);
}

// int msync(void *addr, size_t length, int flags);
long sys_msync(void)
{
    uint64_t addr;
    size_t length;
    int flags;

    if (argu64(0, &addr) < 0 || argu64(1, &length) < 0
     || argint(2, &flags) < 0)
        return -EINVAL;

    return msync((void *)addr, length, flags);
}

long sys_clone()
{
    void *childstk;
    uint64_t flag;
    if (argu64(0, &flag) < 0 || argu64(1, (uint64_t *) & childstk) < 0)
        return -1;
    trace("flags 0x%llx, child stack 0x%p", flag, childstk);
    if (flag != 17) {
        warn("flags other than SIGCHLD are not supported");
        return -1;
    }
    return fork();
}


long sys_wait4()
{
    int pid, opt;
    int *wstatus;
    void *rusage;
    if (argint(0, &pid) < 0 ||
        argu64(1, (uint64_t *) & wstatus) < 0 ||
        argint(2, &opt) < 0 || argu64(3, (uint64_t *) & rusage) < 0)
        return -1;

    // FIXME:
    if (pid != -1 || wstatus != 0 || opt != 0 || rusage != 0) {
        warn("unimplemented. pid %d, wstatus 0x%p, opt 0x%x, rusage 0x%p",
             pid, wstatus, opt, rusage);
        return -1;
    }

    return wait();
}
