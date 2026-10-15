#include <types.h>
#include <sched.h>
#include <trap.h>
#include <console.h>
#include <vm.h>
#include <syscall.h>
//#include <filedesc.h>
//#include <mmap.h>
#include <fs.h>
#include <string.h>
//#include <linux/mman.h>
#include <linux/errno.h>
//#include <linux/signal.h>
//#include <linux/ppoll.h>
#include <linux/time.h>
#include <linux/fcntl.h>
#include <linux/resources.h>

#if 0
int sys_yield(void)
{
    //wfe();
    yield();
    return 0;
}
#endif

// int brk(const void *addr);
size_t sys_brk(uint64_t addr/* uva */)
{
    struct mm_struct *mm = myproc()->mm;
    size_t sz;
    struct trapframe *regs = task_pt_regs(myproc());
    uint64_t newsz;
    int error;

    acquire(&mm->lock);
    sz = mm->sz;

    if ((error = argptr(addr, (char *)&newsz, sizeof(uint64_t))) < 0) {
        release(&mm->lock);
        return error;
    }
    if (newsz == 0) {
        release(&mm->lock);
        return sz;
    }
    int incr = newsz - sz;

    if (incr < 0 && sz + incr < mm->codesz) {
        error("requested brk too small. into code/data region");
        goto bad;
    }

    if (incr > 0 && regs->sp && sz + incr > regs->sp - PAGE_SIZE) {
        error("requested brk %lx too large. too close to sp %lx",
            sz+incr, regs->sp);
        goto bad;
    }

    sz = growproc(mm, incr);
    release(&mm->lock);
    return sz;

bad:
    release(&mm->lock);
    return -1;
}

#if 0
// void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
void *sys_mmap(void)
{
    uint64_t addr;
    size_t length, offset;
    int prot, flags, fd;
    struct vfile *f;

    if (argu64(0, &addr) < 0 ||
        argu64(1, &length) < 0 ||
        argint(2, &prot) < 0 ||
        argint(3, &flags) < 0 ||
        argint(4, &fd) < 0 ||
        argu64(5, &offset) < 0)
        return (void *)-EINVAL;

    trace("addr: 0x%llx, length: 0x%x, prot: 0x%x, flags: 0x%x, fd: %d, offset: 0x%x",
        addr, length, prot, flags, fd, offset);

    if (flags & MAP_ANONYMOUS) {
        if (fd != -1) return (void *)-EINVAL;
        f = NULL;
    } else {
        if (fd < 0 || fd >= OPEN_MAX) return (void *)-EBADF;
        if ((f = get_fd(myproc()->fd_table, fd)) == NULL) return (void *)-EBADF;
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
    if ((flags & MAP_FIXED) && addr == 0) {
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
    return mmap((void *)addr, length, prot, flags, f, offset);
}

// int munmap(void *addr, size_t length);
int sys_munmap(void)
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
int sys_mprotect(void)
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
int sys_msync(void)
{
    uint64_t addr;
    size_t length;
    int flags;

    if (argu64(0, &addr) < 0 || argu64(1, &length) < 0
     || argint(2, &flags) < 0)
        return -EINVAL;

    return msync((void *)addr, length, flags);
}
#endif

// syscall(SYS_clone, flags, stack, ptid, tls, ctid)
int sys_clone(uint64_t flags, uint64_t stack, uint64_t ptid, uint64_t tls, uint64_t ctid)
{
    // ユーザスレッド
    if (flags & (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_THREAD))
        return copy_process(PF_UTHREAD, 0, stack, 0);
    // ユーザプロセス
    else
        return copy_process(0, 0, stack, 0);
}

// FIXME: exit_group should kill every thread in the current thread group.
// void exit_group(int status)
int sys_exit_group(int status) {
    trace("[%d] sys_exit_group: '%s' exit with code %d", myproc()->pid, myproc()->name, status);
    exit_process(status);
    return 0;
}

// void exit(int status)
int sys_exit(int status) {
    trace("[%d] exit '%s' with code %d", myproc()->pid, myproc()->name, status);
    exit_process(status);
    return 0;
}

// pid_t wait4(pid_t pid, int *status, int options, struct rusage *rusage);
int sys_wait4(int pid, uint64_t statusp/* uva */, int options, uint64_t rusagep/*uva*/)
{
    struct rusage rusage;
    int status;
    int error;

    trace("[%d] pid: %d, statusp: 0x%lx, options: 0x%x, rusagep: 0x%lx", pid, statusp, options, rusagep);

    if ((error = argptr(statusp, (char *)&status, sizeof(int))) < 0)
        return error;
    if ((error = argptr(rusagep, (char *)&rusage, sizeof(rusage))) < 0)
        return error;

    if (options) {
        error("options are not supported");
        return -EINVAL;
    }
    return wait4(pid, &status, options, &rusage);
}

#if 0
// int kill(pid_t pid, int sig);
int sys_kill(void)
{
    int pid, sig;

    if (argint(0, &pid) < 0 || argint(1, &sig) < 0)
        return -EINVAL;

    if (sig < 1 || sig >= NSIG)
        return -EINVAL;

    trace("pid=%d, sig=%d", pid, sig);

    return kill(pid, sig);
}

// int tkill(int tid, int sig);
int sys_tkill(void)
{
    return 0;
}

// int sigsuspend(const sigset_t *mask);
int sys_rt_sigsuspend(void)
{
    sigset_t *mask;
    if (argptr(0, (void **)&mask, sizeof(sigset_t)) < 0)
        return -EINVAL;
    return sigsuspend(mask);
}

// int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sys_rt_sigaction(void)
{
    int signum;
    struct k_sigaction *act, *oldact;

    if (argint(0, &signum) < 0 || argptr(1, (void **)&act, sizeof(struct k_sigaction)) < 0
     || argptr(2, (void **)&oldact, sizeof(struct k_sigaction)) < 0)
        return -EINVAL;
#if 0
    trace("sig=%d, act=%p, oldact=%p", signum, act, oldact);
    if (act)
        trace("act->sa_handler: %p", act->handler);
#endif
    if (signum < 1 || signum >= NSIG || signum == SIGSTOP || signum == SIGKILL)
        return -EINVAL;

    return sigaction(signum, act, oldact);
}

// int sigpending(sigset_t *set);
int sys_rt_sigpending(void)
{
    sigset_t *pending;

    if (argptr(0, (void **)&pending, sizeof(sigset_t)) < 0)
        return -EINVAL;

    return sigpending(pending);
}

// int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
// rt_sigprocmask()には第4の引数 size_t sizeがあり
int sys_rt_sigprocmask(void)
{
    int how;
    sigset_t *set, *oldset;
    size_t size;

    if (argint(0, &how) < 0 || argptr(1, (void **)&set, sizeof(sigset_t)) < 0
     || argptr(2, (void **)&oldset, sizeof(sigset_t)) < 0 || argu64(3, &size) < 0)
        return -EINVAL;

    trace("[%d] how=%d, *set=0x%llx, oldset=%p, size=%lld", myproc()->pid, how, set ? *set : 0, oldset, size);

    return sigprocmask(how, set, oldset, size);
}

// int sigreturn(...);
int sys_rt_sigreturn(void)
{
    return sigreturn();
}

// int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts, const sigset_t *sigmask);
int sys_ppoll(void)
{
    struct pollfd *fds;
    nfds_t nfds;
    struct timespec *timeout_ts;
    sigset_t *sigmask;

    if (argu64(1, &nfds) < 0
     || argptr(0, (void **)&fds, nfds * sizeof(struct pollfd)) < 0
     || argptr(2, (void **)&timeout_ts, sizeof(struct timespec)) < 0
     || argptr(3, (void **)&sigmask, sizeof(sigset_t)) < 0)
        return -EINVAL;

    trace("fds: %p, nfds: 0x%lld, timeout: %p, sigmask: %p", fds, nfds, timeout_ts, sigmask);

    return ppoll(fds, nfds, timeout_ts, sigmask);
}

/* 呼び出し元プロセスのファイルモード作成マスク (umask) を mask & 0777 に設定し
 * 変更前のマスク値を返す */
//  mode_t umask(mode_t mask);
mode_t sys_umask(void)
{
    mode_t umask;
    mode_t oumask = myproc()->umask;

    if (argint(0, (int *)&umask) < 0)
        return -EINVAL;

    myproc()->umask = umask & S_IRWXUGO;
    return oumask;
}

// FIXME: use pid instead of tid since we don't have threads :)
// pid_t set_tid_address(int *tidptr);
int sys_set_tid_address(void) {
    trace("set_tid_address: name '%s'", myproc()->name);
    return myproc()->pid;
}

/* 呼び出し元のプロセスのpidを取得する */
// pid_t getpid(void);
int sys_getpid(void) {
    return myproc()->pid;
}

/* 呼び出し元のスレッド ID (TID) を取得する */
// pid_t gettid(void);
int sys_gettid(void) {
    trace("gettid: name '%s'", myproc()->name);
    return myproc()->pid;
}

/* pid によって識別されるプロセスのプロセスグループを取得する */
// pid_t getpgid(pid_t pid);
int sys_getpgid(void)
{
    pid_t pid;

    if (argint(0, &pid) < 0)
        return -EINVAL;

    return getpgid(pid);
}

/*  pid で指定したプロセスの PGID に pgid を設定する */
// int setpgid(pid_t pid, pid_t pgid);
int sys_setpgid(void)
{
    pid_t pid, pgid;

    if (argint(0, &pid) < 0 || argint(1, &pgid) < 0)
        return -EINVAL;

    return setpgid(pid, pgid);
}

/* 呼び出し元の親プロセスのpidを取得する */
// pid_t getppid(void);
int sys_getppid(void)
{
    return myproc()->parent->pid;
}

static inline void cap_emulate_setxuid(int old_ruid, int old_euid, int old_suid)
{
    struct task_struct *p = myproc();

    if ((old_ruid == 0 || old_euid == 0 || old_suid == 0) &&
        (p->uid != 0 && p->euid != 0 && p->suid != 0)) {
        cap_clear(p->cap_permitted);
        cap_clear(p->cap_effective);
    }
    if (old_euid == 0 && p->euid != 0) {
        cap_clear(p->cap_effective);
    }
    if (old_euid != 0 && p->euid == 0) {
        p->cap_effective = p->cap_permitted;
    }
}

/* ユーザIDを設定する */
//  int setuid(uid_t uid);
int sys_setuid(void)
{
    struct task_struct *p = myproc();
    uid_t uid, old_ruid, old_euid, old_suid, new_ruid, new_suid;

    if (argint(0, (int *)&uid) < 0)
        return -EINVAL;

    old_euid = p->euid;
    old_ruid = new_ruid = p->uid;
    new_suid = old_suid = p->suid;

    trace("uid: %d, old: euid=%d ruid=%d suid=%d, new: ruid=%d suid=%d",
        uid, old_euid, old_ruid, old_suid, new_ruid, new_suid);
    trace("p[%d] cap_effective=%d", p->pid, p->cap_effective);

    if (capable(CAP_SETUID)) {
        if (uid != old_ruid) {
            p->uid = uid;
            new_suid = uid;
        } else if ((uid != p->uid) && (uid != new_suid)) {
            return -EPERM;
        }
    }

    if (old_euid != uid) disb();
    p->fsuid = p->euid = uid;
    p->suid = new_suid;

    cap_emulate_setxuid(old_ruid, old_euid, old_suid);

    return 0;
}

/* 実 (real)ユーザIDと実効 (effective)ユーザIDを設定する */
// int setreuid(uid_t ruid, uid_t euid);
int sys_setreuid(void)
{
    struct task_struct *p = myproc();
    uid_t ruid, euid, old_ruid, old_euid, old_suid, new_ruid, new_euid;

    if (argint(0, (int *)&ruid) < 0 || argint(1, (int *)&euid) < 0)
        return -EINVAL;

    new_ruid = old_ruid = p->uid;
    new_euid = old_euid = p->euid;
    old_suid = p->suid;

    if (ruid != (uid_t)-1) {
        new_ruid = ruid;
        if ((old_ruid != ruid) && (p->euid != ruid) && !capable(CAP_SETUID))
            return -EPERM;
    }

    if (euid != (uid_t)-1) {
        new_euid = euid;
        if ((old_euid != euid) && (p->suid != euid) && !capable(CAP_SETUID))
            return -EPERM;
    }

    if (new_ruid != old_ruid)
        p->uid = ruid;

    if (new_euid != old_euid) disb();
    p->fsuid = p->euid = new_euid;

    if (ruid != (uid_t)-1 || (euid != (uid_t)-1 && euid != old_ruid))
        p->suid = p->euid;
    p->fsuid = p->euid;

    cap_emulate_setxuid(old_ruid, old_euid, old_suid);

    return 0;
}

/* ユーザの実ID、実効ID、保存IDを設定する */
// int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int sys_setresuid(void)
{
    struct task_struct *p = myproc();
    uid_t ruid, euid, suid;
    uid_t old_ruid = p->uid, old_euid = p->euid, old_suid= p->suid;

    if (argint(0, (int *)&ruid) < 0 || argint(1, (int *)&euid) < 0
     || argint(2, (int *)&suid) < 0)
        return -EINVAL;

    if (!capable(CAP_SETUID)) {
        if ((ruid != (uid_t)-1) && (ruid != p->uid) &&
            (ruid != p->euid) && (ruid != p->suid))
            return -EPERM;
        if ((euid != (uid_t)-1) && (euid != p->uid) &&
            (euid != p->euid) && (euid != p->suid))
            return -EPERM;
        if ((suid != (uid_t)-1) && (suid != p->uid) &&
            (suid != p->euid) && (suid != p->suid))
            return -EPERM;
    }

    if (ruid != (uid_t)-1)
        p->uid = ruid;

    if (euid != (uid_t)-1) {
        if (euid != p->euid) disb();
        p->euid = euid;
        p->fsuid = euid;
    }
    if (suid != (uid_t)-1)
        p->suid = suid;

    cap_emulate_setxuid(old_ruid, old_euid, old_suid);

    return 0;
}

/* ファイルシステムのチェックに用いられるユーザIDを設定する */
// int setfsuid(uid_t fsuid);
int sys_setfsuid(void)
{
    struct task_struct *p = myproc();

    uid_t fsuid, old_fsuid = p->fsuid;

    if (argint(0, (int *)&fsuid) < 0)
        return -EINVAL;

    if (fsuid == p->uid || fsuid == p->euid || fsuid == p->suid
     || fsuid == p->fsuid || capable(CAP_SETUID)) {
         if (fsuid != old_fsuid) disb();
         p->fsuid = fsuid;
    }

    if (old_fsuid == 0 && p->fsuid != 0)
        cap_t(p->cap_effective) &= ~CAP_FS_MASK;
    if (old_fsuid != 0 && p->fsuid == 0)
        cap_t(p->cap_effective) |= (cap_t(p->cap_permitted) & CAP_FS_MASK);

    return old_fsuid;
}

/* グループIDを設定する */
// int setgid(gid_t gid);
int sys_setgid(void)
{
    struct task_struct *p = myproc();
    gid_t gid, old_egid = p->egid;

    if (argint(0, (int *)&gid) < 0)
        return -EINVAL;

    if (capable(CAP_SETGID)) {
        if (old_egid != gid) disb();
        p->gid = p->egid = p->sgid = gid;
    } else if ((gid == p->gid) || (gid == p->sgid)) {
        if (old_egid != gid) disb();
        p->egid = gid;
    } else {
        return -EPERM;
    }

    return 0;
}

/* 実グループIDと実効グループIDを設定する */
// int setregid(gid_t rgid, gid_t egid);
int sys_setregid(void)
{
    struct task_struct *p = myproc();
    gid_t rgid, egid;
    gid_t old_rgid = p->gid, old_egid = p->egid;
    gid_t new_rgid, new_egid;

    if (argint(0, (int *)&rgid) < 0 || argint(1, (int *)&egid) < 0)
        return -EINVAL;

    new_rgid = old_rgid;
    new_egid = old_egid;

    if (rgid != (gid_t)-1) {
        if ((old_rgid == rgid) || (p->egid == rgid) || capable(CAP_SETGID))
            new_rgid = rgid;
        else
            return -EPERM;
    }
    if (egid != (gid_t)-1) {
        if ((old_rgid == egid) || (p->egid == egid) || (p->sgid == egid)
          || capable(CAP_SETGID))
            new_egid = egid;
        else
            return -EPERM;
    }

    if (new_egid != old_egid) disb();
    if (rgid != (gid_t)-1 || (egid != (gid_t)-1 && egid != old_rgid))
        p->sgid = new_egid;
    p->fsgid = new_egid;
    p->egid = new_egid;
    p->gid = new_rgid;

    return 0;
}

/* 実グループID、実効グループID、保存グループIDを設定する */
// int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int sys_setresgid(void)
{
    struct task_struct *p = myproc();
    gid_t rgid, egid, sgid;

    if (argint(0, (int *)&rgid) < 0 || argint(1, (int *)&egid) < 0
     || argint(2, (int *)&sgid) < 0)
        return -EINVAL;

    if (!capable(CAP_SETGID)) {
        if ((rgid != (gid_t)-1) && (rgid != p->gid) &&
            (rgid != p->egid) && (rgid != p->sgid))
            return -EPERM;
        if ((egid != (gid_t)-1) && (egid != p->gid) &&
            (egid != p->egid) && (egid != p->sgid))
            return -EPERM;
        if ((sgid != (gid_t)-1) && (sgid != p->gid) &&
            (sgid != p->egid) && (sgid != p->sgid))
            return -EPERM;
    }

    if (egid != (gid_t)-1) {
        if (egid != p->egid) disb();
        p->egid = egid;
        p->fsgid = egid;
    }

    if (rgid != (gid_t)-1)
        p->gid = rgid;
    if (sgid != (gid_t)-1)
        p->sgid = sgid;

    return 0;
}

/* ファイルシステムのチェックに用いられるグループIDを設定する */
// int setfsgid(uid_t fsgid);
int sys_setfsgid(void)
{
    struct task_struct *p = myproc();
    gid_t fsgid, old_fsgid = p->fsgid;

    if (argint(0, (int *)&fsgid) < 0)
        return -EINVAL;

    if (fsgid == p->gid || fsgid == p->egid || fsgid == p->sgid
     || fsgid == p->fsgid || capable(CAP_SETGID)) {
         if (fsgid != old_fsgid) disb();
         p->fsgid = fsgid;
    }
    return old_fsgid;
}

/* 実ユーザID、実効ユーザID、保存ユーザIDを取得する */
// int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
int sys_getresuid(void)
{
    struct task_struct *p = myproc();
    uid_t *ruid, *euid, *suid;

    if (argptr(0, (void **)&ruid, sizeof(uid_t)) < 0
     || argptr(1, (void **)&euid, sizeof(uid_t)) < 0
     || argptr(2, (void **)&suid, sizeof(uid_t)) < 0)
        return -EINVAL;

    *ruid = p->uid;
    *euid = p->euid;
    *suid = p->suid;

    return 0;
}

/* 実グループID、実効グループID、保存グループIDを取得する */
// int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
int sys_getresgid(void)
{
    struct task_struct *p = myproc();
    gid_t *rgid, *egid, *sgid;

    if (argptr(0, (void **)&rgid, sizeof(gid_t)) < 0
     || argptr(1, (void **)&egid, sizeof(gid_t)) < 0
     || argptr(2, (void **)&sgid, sizeof(gid_t)) < 0)
        return -EINVAL;

    *rgid = p->gid;
    *egid = p->egid;
    *sgid = p->sgid;

    return 0;
}

/* ユーザIDを取得する */
// uid_t getuid(void);
int sys_getuid(void)
{
    return myproc()->uid;
}

/* 実効ユーザIDを取得する */
// uid_t geteuid(void);
int sys_geteuid(void)
{
    return myproc()->euid;
}

/* グループIDを取得する */
// gid_t getgid(void);
int sys_getgid(void)
{
    return myproc()->gid;
}

/* 実効グループIDを取得する */
// gid_t getegid(void);
int sys_getegid(void)
{
    return myproc()->egid;
}

/* 補助グループIDのリストを取得する */
//  int getgroups(int size, gid_t list[]);
int sys_getgroups(void)
{
    struct task_struct *p = myproc();
    size_t size;
    gid_t *list;
    int ngroups = p->ngroups;

    if (argu64(0, &size) < 0) return -EINVAL;
    if (size < 0) return -EINVAL;
    if (size == 0) return ngroups;
    if (ngroups > size) return -EINVAL;

    if (argptr(1, (void **)&list, sizeof(gid_t) * size) < 0)
        return -EINVAL;

    memmove(list, p->groups, sizeof(gid_t) * ngroups);
    return ngroups;
}

/* 補助グループIDのリストを設定する */
//  int setgroups(size_t size, const gid_t *list);
int sys_setgroups()
{
    struct task_struct *p = myproc();
    size_t size;
    gid_t *list;

    if (argu64(0, &size) < 0) return -EINVAL;
    if (size < 0) return -EINVAL;
    if (size > NGROUPS) return -EINVAL;
    if (!capable(CAP_SETGID)) return -EPERM;

    if (argptr(1, (void **)&list, sizeof(gid_t) * size) < 0)
        return -EINVAL;

    if (size == 0 && list == NULL) {
        memset(p->groups, 0, sizeof(gid_t) * p->ngroups);
        p->ngroups = 0;
        return 0;
    }

    if (size > 0 && list != NULL) {
        memmove(p->groups, list, sizeof(gid_t) * size);
        p->ngroups = size;
    }

    return 0;
}

// int getitimer(int which, struct itimerval *value);
int sys_getitimer(void)
{
    int which;
    struct itimerval *value;
    long err;

    if (argint(0, &which) < 0) {
        return -EINVAL;
    }

    if ((err = argptr(1, (void **)&value, sizeof(struct itimerval))) < 0)
        return err;

    trace("which: %d, value: %p", which, value);

    return getitimer(which, value);
}

// int setitimer(int which, const struct itimerval *value, struct itimerval *ovalue);
int sys_setitimer(void)
{
    int which;
    struct itimerval *new_value, *old_value;
    long err;

    if (argint(0, &which) < 0) {
        return -EINVAL;
    }

    if ((err = argptr(1, (void **)&new_value, sizeof(struct itimerval))) < 0)
        return err;

    if ((err = argptr(2, (void **)&old_value, sizeof(struct itimerval))) < 0)
        return err;

    trace("which: %d, new_value: inter->sec: 0x%llx, inter->usec: 0x%llx, val->sec: 0x%llx, val->usec: 0x%llx", which, new_value->it_interval.tv_sec, new_value->it_interval.tv_usec, new_value->it_value.tv_sec, new_value->it_value.tv_usec);

    return setitimer(which, new_value, old_value);
}

#endif
