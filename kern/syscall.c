#include <types.h>
#include <param.h>
#include <syscall.h>
#include <linux/syscall.h>
#include <memlayout.h>
#include <trap.h>
#include <console.h>
#include <proc.h>
#include <linux/errno.h>

typedef long (*func)();

/* Check if a block of memory lies within the process user space. */
int in_user(void *s, size_t n)
{
    struct proc *p = thisproc();
    if ((p->base <= (uint64_t) s && (uint64_t) s + n <= p->sz) ||
        (USER_TOP - p->stksz <= (uint64_t) s
         && (uint64_t) s + n <= USER_TOP))
        return 1;
    return 0;
}

/*
 * Fetch the nul-terminated string at addr from the current process.
 * Doesn't actually copy the string - just sets *pp to point at it.
 * Returns length of string, not including nul.
 */
long fetchstr(uint64_t addr, char **pp)
{
    struct proc *p = thisproc();
    char *s;
    *pp = s = (char *)addr;
    if (p->base <= addr && addr < p->sz) {
        for (; (uint64_t) s < p->sz; s++)
            if (*s == 0)
                return s - *pp;
    } else if (USER_TOP - p->stksz <= addr && addr < USER_TOP) {
        for (; (uint64_t) s < USER_TOP; s++)
            if (*s == 0)
                return s - *pp;
    }
    return -1;
}

/*
 * Fetch the nth (starting from 0) 32-bit system call argument.
 * In our ABI, x8 contains system call index, x0-x5 contain parameters.
 * now we support system calls with at most 6 parameters.
 */
long argint(int n, int *ip)
{
    struct proc *proc = thisproc();
    if (n > 5) {
        warn("too many system call parameters");
        return -1;
    }
    *ip = proc->tf->x[n];

    return 0;
}

/*
 * Fetch the nth (starting from 0) 64-bit system call argument.
 * In our ABI, x8 contains system call index, x0-x5 contain parameters.
 * now we support system calls with at most 6 parameters.
 */
long argu64(int n, uint64_t * ip)
{
    struct proc *proc = thisproc();
    if (n > 5) {
        warn("too many system call parameters");
        return -1;
    }
    *ip = proc->tf->x[n];

    return 0;
}

/*
 * Fetch the nth word-sized system call argument as a pointer
 * to a block of memory of size bytes. Check that the pointer
 * lies within the process address space.
 */
long argptr(int n, char **pp, size_t size)
{
    uint64_t i = 0;
    if (argu64(n, &i) < 0) {
        return -1;
    }
    if (in_user((void *)i, size)) {
        *pp = (char *)i;
        return 0;
    } else {
        return -1;
    }
}

/*
 * Fetch the nth word-sized system call argument as a string pointer.
 * Check that the pointer is valid and the string is nul-terminated.
 * (There is no shared writable memory, so the string can't change
 * between this check and being used by the kernel.)
 */
long argstr(int n, char **pp)
{
    uint64_t addr = 0;
    if (argu64(n, &addr) < 0)
        return -1;
    int r = fetchstr(addr, pp);
    return r;
}

// FIXME: use pid instead of tid since we don't have threads :)
long sys_set_tid_address() {
    trace("set_tid_address: name '%s'", thisproc()->name);
    return thisproc()->pid;
}

long sys_gettid() {
    trace("gettid: name '%s'", thisproc()->name);
    return thisproc()->pid;
}

// FIXME: always return 0 since we don't have signals  :)
long sys_rt_sigprocmask() {
    trace("rt_sigprocmask: name '%s' how 0x%x", thisproc()->name,
            (int)thisproc()->tf->x[0]);
    return 0;
}

// FIXME: exit_group should kill every thread in the current thread group.
long sys_exit_group() {
    trace("sys_exit_group: '%s' exit with code %d", thisproc()->name, thisproc()->tf->x[0]);
    exit(thisproc()->tf->x[0]);
    return 0;
}

long sys_exit() {
    trace("sys_exit: '%s' exit with code %d", thisproc()->name, thisproc()->tf->x[0]);
    exit(thisproc()->tf->x[0]);
    return 0;
}

static func syscalls[] = {
//    [SYS_getcwd] = (func)sys_getcwd,            // 17
    [SYS_dup] = sys_dup,                        // 23
//    [SYS_dup3] = sys_dup3,                      // 24
    [SYS_fcntl] = sys_fcntl,                    // 25
    [SYS_ioctl] = sys_ioctl,                    // 29
    [SYS_mknodat] = sys_mknodat,                // 33
    [SYS_mkdirat] = sys_mkdirat,                // 34
    [SYS_unlinkat] = sys_unlinkat,              // 35
//    [SYS_symlinkat] = sys_symlinkat,            // 36
//    [SYS_linkat] = sys_linkat,                  // 37
//    [SYS_umount2] = sys_umount2,                // 39
//    [SYS_mount] = sys_mount,                    // 40
//    [SYS_faccessat] = sys_faccessat,            // 48
    [SYS_chdir] = sys_chdir,                    // 49
//    [SYS_fchmodat] = sys_fchmodat,              // 53
//    [SYS_fchownat] = sys_fchownat,              // 54
//    [SYS_fchown] = sys_fchown,                  // 55
    [SYS_openat] = sys_openat,                  // 56
    [SYS_close] = sys_close,                    // 57
    [SYS_pipe2] = sys_pipe2,                    // 59
    [SYS_getdents64] = sys_getdents64,            // 61
    [SYS_lseek] = (func)sys_lseek,              // 62
    [SYS_read] = (func)sys_read,                // 63
    [SYS_write] = (func)sys_write,              // 64
//    [SYS_readv] = (func)sys_readv,              // 65
    [SYS_writev] = (func)sys_writev,            // 66
//    [SYS_readlinkat] = (func)sys_readlinkat,    // 78
    [SYS_newfstatat] = sys_fstatat,             // 79
    [SYS_fstat] = sys_fstat,                    // 80
//    [SYS_fsync] = sys_fsync,                    // 82
//    [SYS_fdatasync] = sys_fdatasync,            // 83
//    [SYS_utimensat] = sys_utimensat,            // 88
    [SYS_exit] = sys_exit,                      // 93
    // FIXME: exit_group should kill every thread in the current thread group.
    [SYS_exit_group] = sys_exit_group,          // 94
    [SYS_set_tid_address] = sys_set_tid_address,  // 96
//    [SYS_nanosleep] = sys_nanosleep,            // 101
//    [SYS_getitimer] = sys_getitimer,            // 102
//    [SYS_setitimer] = sys_setitimer,            // 103
//    [SYS_clock_settime] = sys_clock_settime,    // 112
//    [SYS_clock_gettime] = sys_clock_gettime,    // 113
//    [SYS_sched_getaffinity] = sys_sched_getaffinity, // 123
    [SYS_sched_yield] = sys_yield,              // 124
//    [SYS_kill] = sys_kill,                      // 129
//    [SYS_rt_sigsuspend] = sys_rt_sigsuspend,    // 133
///   [SYS_rt_sigaction] = sys_rt_sigaction,      // 134
    [SYS_rt_sigprocmask] = sys_rt_sigprocmask,  // 135
//    [SYS_rt_sigpending] = sys_rt_sigpending,    // 136
//    [SYS_rt_sigreturn] = sys_rt_sigreturn,      // 139
//    [SYS_setregid] = sys_setregid,              // 143
//    [SYS_setgid] = sys_setgid,                  // 144
//    [SYS_setreuid] = sys_setreuid,              // 145
//    [SYS_setuid] = sys_setuid,                  // 146
//    [SYS_setresuid] = sys_setresuid,            // 147
//    [SYS_getresuid] = sys_getresuid,            // 148
//    [SYS_setresgid] = sys_setresgid,            // 149
//    [SYS_getresgid] = sys_getresgid,            // 150
//    [SYS_setfsuid] = sys_setfsuid,              // 151
//    [SYS_setfsgid] = sys_setfsgid,              // 152
//    [SYS_setpgid] = sys_setpgid,                // 154
//    [SYS_getpgid] = sys_getpgid,                // 155
//    [SYS_getgroups] = sys_getgroups,            // 158
//    [SYS_setgroups] = sys_setgroups,            // 159
//    [SYS_uname] = sys_uname,                    // 160
//    [SYS_umask] = (func)sys_umask,              // 166
//    [SYS_getpid] = sys_getpid,                  // 172
//    [SYS_getppid] = sys_getppid,                // 173
//    [SYS_getuid] = sys_getuid,                  // 174
//    [SYS_geteuid] = sys_geteuid,                // 175
//    [SYS_getgid] = sys_getgid,                  // 176
//    [SYS_getegid] = sys_getegid,                // 177
    [SYS_gettid] = sys_gettid,                  // 178
//    [SYS_sysinfo] = sys_sysinfo,                // 179
    [SYS_brk] = (func)sys_brk,                  // 214
    [SYS_munmap] = sys_munmap,                  // 215
    [SYS_mremap] = (func)sys_mremap,            // 216
    [SYS_clone] = sys_clone,                    // 220
    [SYS_execve] = sys_execve,                  // 221
    [SYS_mmap] = (func)sys_mmap,                // 222
//    [SYS_fadvise64] = sys_fadvise64,            // 223
    [SYS_mprotect] = sys_mprotect,              // 226
    [SYS_msync] = sys_msync,                    // 227
//    [SYS_madvise] = sys_madvise,                // 233
    [SYS_wait4] = sys_wait4,                    // 260
//    [SYS_prlimit64] = sys_prlimit64,            // 261
//    [SYS_renameat2] = sys_renameat2,            // 276
//    [SYS_getrandom] = sys_getrandom,            // 278
//    [SYS_faccessat2] = sys_faccessat2,          // 439
};

__attribute__((unused)) static char *syscall_names[] = {
    [SYS_getcwd] = "sys_getcwd",                  // 17
    [SYS_dup] = "sys_dup",                        // 23
    [SYS_dup3] = "sys_dup3",                      // 24
    [SYS_fcntl] = "sys_fcntl",                    // 25
    [SYS_ioctl] = "sys_ioctl",                    // 29
    [SYS_mknodat] = "sys_mknodat",                // 33
    [SYS_mkdirat] = "sys_mkdirat",                // 34
    [SYS_unlinkat] = "sys_unlinkat",              // 35
    [SYS_symlinkat] = "sys_symlinkat",            // 36
    [SYS_linkat] = "sys_linkat",                  // 37
    [SYS_umount2] = "sys_umount2",                // 39
    [SYS_mount] = "sys_mount",                    // 40
    [SYS_faccessat] = "sys_faccessat",            // 48
    [SYS_chdir] = "sys_chdir",                    // 49
    [SYS_fchmodat] = "sys_fchmodat",              // 53
    [SYS_fchownat] = "sys_fchownat",              // 54
    [SYS_fchown] = "sys_fchown",                  // 55
    [SYS_openat] = "sys_openat",                  // 56
    [SYS_close] = "sys_close",                    // 57
    [SYS_pipe2] = "sys_pipe2",                    // 59
    [SYS_getdents64] = "sys_getdents64",          // 61
    [SYS_lseek] = "sys_lseek",                    // 62
    [SYS_read] = "sys_read",                      // 63
    [SYS_write] = "sys_write",                    // 64
    [SYS_readv] = "sys_readv",                    // 65
    [SYS_writev] = "sys_writev",                  // 66
    [SYS_readlinkat] = "sys_readlinkat",          // 78
    [SYS_newfstatat] = "sys_fstatat",             // 79
    [SYS_fstat] = "sys_fstat",                    // 80
    [SYS_fstat] = "sys_fstat",                    // 80
    [SYS_fsync] = "sys_fsync",                    // 82
    [SYS_fdatasync] = "sys_fdatasync",            // 83
    [SYS_utimensat] = "sys_utimensat",            // 88
    [SYS_exit] = "sys_exit",                      // 93
    [SYS_exit_group] = "sys_exit",                // 94
    [SYS_set_tid_address] = "sys_gettid",         // 96
    [SYS_nanosleep] = "sys_nanosleep",            // 101
    [SYS_getitimer] = "sys_getitimer",            // 102
    [SYS_setitimer] = "sys_setitimer",            // 103
    [SYS_clock_settime] = "sys_clock_settime",    // 112
    [SYS_clock_gettime] = "sys_clock_gettime",    // 113
    [SYS_sched_getaffinity] = "sys_sched_getaffinity", // 123
    [SYS_sched_yield] = "sys_yield",              // 124
    [SYS_kill] = "sys_kill",                      // 129
    [SYS_rt_sigsuspend] = "sys_rt_sigsuspend",    // 133
    [SYS_rt_sigaction] = "sys_rt_sigaction",      // 134
    [SYS_rt_sigprocmask] = "sys_rt_sigprocmask",  // 135
    [SYS_rt_sigpending] = "sys_rt_sigpending",    // 136
    [SYS_rt_sigreturn] = "sys_rt_sigreturn",      // 139
    [SYS_setregid] = "sys_setregid",              // 143
    [SYS_setgid] = "sys_setgid",                  // 144
    [SYS_setreuid] = "sys_setreuid",              // 145
    [SYS_setuid] = "sys_setuid",                  // 146
    [SYS_setresuid] = "sys_setresuid",            // 147
    [SYS_getresuid] = "sys_getresuid",            // 148
    [SYS_setresgid] = "sys_setresgid",            // 149
    [SYS_getresgid] = "sys_getresgid",            // 150
    [SYS_setfsuid] = "sys_setfsuid",              // 151
    [SYS_setfsgid] = "sys_setfsgid",              // 152
    [SYS_setpgid] = "sys_setpgid",                // 154
    [SYS_getpgid] = "sys_getpgid",                // 155
    [SYS_getgroups] = "sys_getgroups",            // 158
    [SYS_setgroups] = "sys_setgroups",            // 159
    [SYS_uname] = "sys_uname",                    // 160
    [SYS_umask] = "sys_umask",                    // 166
    [SYS_getpid] = "sys_getpid",                  // 172
    [SYS_getppid] = "sys_getppid",                // 173
    [SYS_getuid] = "sys_getuid",                  // 174
    [SYS_geteuid] = "sys_geteuid",                // 175
    [SYS_getgid] = "sys_getgid",                  // 176
    [SYS_getegid] = "sys_getegid",                // 177
    [SYS_gettid] = "sys_gettid",                  // 178
    [SYS_sysinfo] = "sys_sysinfo",                // 179
    [SYS_brk] = "sys_brk",                        // 214
    [SYS_munmap] = "sys_munmap",                  // 215
    [SYS_mremap] = "sys_mremap",                  // 216
    [SYS_clone] = "sys_clone",                    // 220
    [SYS_execve] = "sys_execve",                  // 221
    [SYS_mmap] = "sys_mmap",                      // 222
    [SYS_fadvise64] = "sys_fadvise64",            // 223
    [SYS_mprotect] = "sys_mprotect",              // 226
    [SYS_msync] = "sys_msync",                    // 227
    [SYS_madvise] = "sys_madvise",                // 233
    [SYS_wait4] = "sys_wait4",                    // 260
    [SYS_prlimit64] = "sys_prlimit64",            // 261
    [SYS_renameat2] = "sys_renameat2",            // 276
    [SYS_getrandom] = "sys_getrandom",            // 278
    [SYS_faccessat2] = "sys_faccessat2",          // 439
};


long syscall1(struct trapframe *tf)
{
    thisproc()->tf = tf;
    int sysno = tf->x[8];
    long ret = -EINVAL;

    if (sysno > 0 && sysno < ARRAY_SIZE(syscalls) && syscalls[sysno]) {
        if (sysno != SYS_sched_yield && sysno != SYS_rt_sigprocmask)
            trace("proc[%d] %s called", thisproc()->pid, syscall_names[sysno]);
        ret = syscalls[sysno]();
    } else {
        //debug_reg();
        if (syscall_names[sysno]) {
            panic("Unexpected syscall #%d (%s)\n", sysno, syscall_names[sysno]);
        } else {
            panic("Unexpected syscall #%d (unknown)\n", sysno);
        }
    }
    return ret;
}
