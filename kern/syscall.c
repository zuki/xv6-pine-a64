#include <types.h>
#include <syscall.h>
#include <memlayout.h>
#include <trap.h>
#include <console.h>
#include <sched.h>
//#include <debug.h>
#include <syscall.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <clock.h>
#include <string.h>
#include <rtc.h>
#include <vm.h>
//#include <fs.h>
//#include <filedesc.h>
//#include <file.h>


/* メモリがユーザ空間にあるかチェック: あれば 1, なければ 0 */
int in_user(uint64_t addr, size_t n)
{
    struct mm_struct *mm = myproc()->mm;

    if (addr + n <= mm->sz)
        return 1;

    // TODO: vmaを作成したら修正
#if 0
    struct vma *vma = mm->vmas;
    while (vma) {
        if ((uint64_t)vma->addr <= (uint64_t)s
         && ((uint64_t)s + n) <= ((uint64_t)vma->addr + vma->length))
            return 1;
        vma = vma->next;
    }
#endif
    return 0;
}

/// カレントプロセスのaddrにあるNULL終端の文字列を取得する
// nullを含まない文字列の長さを返す。maxになったらエラー。
// エラーの場合は-1を返す.
int argstr(uint64_t addr, char *buf, int max)
{
    if (copyinstr(myproc()->mm, buf, addr, max) < 0)
        return -1;
    return strlen(buf);
}

// 引数addrをsizeバイトのブロックへのユーザ空間のアドレスとして
// カーネル空間のppにcopinする
//  *p = 0 の場合は成功とする

int argptr(uint64_t addr, char *pp, size_t size)
{
    if (addr == 0) {
        pp = (char *)0;
        return 0;
    }

    if (in_user(addr, size)) {
        if (copyin(myproc()->mm, pp, addr, size) < 0)
            return -1;
        return 0;
    }
    return -1;
}

#if 0
// int clock_gettime(clockid_t clock_id, struct timespec *tp);
int sys_clock_gettime(clockid_t clock_id, uint64_t tpp)
{
    struct timespec *tp = (struct timespec *)tpp;
    static int count = 0;

    if ((++count % 1000) == 0)
        trace("[%d] clk_id: %d, tp: 0x%p\n", count, clk_id, tp);

    return clock_gettime(clk_id, tp);
}

int sys_clock_settime(void)
{
    clockid_t clk_id;
    struct timespec *tp;

    if (argint(0, (clockid_t *)&clk_id) < 0 || argptr(1, (void **)&tp, sizeof(struct timespec)) < 0)
        return -EINVAL;

    trace("clk_id: %d, tp: 0x%p\n", clk_id, tp);

#ifdef USING_RASPI
    rtc_settime(tp);
#endif

    return clock_settime(clk_id, tp);
}

//  int nanosleep(const struct timespec *req, struct timespec *rem);
int sys_nanosleep(void)
{
    struct timespec *req, *rem, t;
    uint64_t expire;

    if (argptr(0, (void **)&req, sizeof(struct timespec)) < 0
     || argptr(1, (void **)&rem, sizeof(struct timespec)) < 0)
        return -EINVAL;

    memmove(&t, req, sizeof(struct timespec));

    if (t.tv_nsec >= 1000000000L || t.tv_nsec < 0 || t.tv_sec < 0)
        return -EINVAL;

    expire = t.tv_sec * 1000000 + (t.tv_nsec + 999) / 1000;
    trace("sec: %d, nsec: %d, expire: %d", t.tv_sec, t.tv_nsec, expire);
    delay_us(expire);

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return 0;
}
#endif

void * const syscalls[] = {
    //[SYS_getcwd] = (func)sys_getcwd,            // 17
    [SYS_dup] = sys_dup,                        // 23
    [SYS_dup3] = sys_dup3,                      // 24
    [SYS_fcntl] = sys_fcntl,                    // 25
    [SYS_ioctl] = sys_ioctl,                    // 29
    [SYS_mknodat] = sys_mknodat,                // 33
    [SYS_mkdirat] = sys_mkdirat,                // 34
    [SYS_unlinkat] = sys_unlinkat,              // 35
    [SYS_symlinkat] = (func)sys_symlinkat,            // 36
    [SYS_linkat] = sys_linkat,                  // 37
    [SYS_renameat] = sys_renameat,              // 38
    //[SYS_umount2] = sys_umount2,                // 39
    //[SYS_mount] = sys_mount,                    // 40
    //[SYS_faccessat] = sys_faccessat,            // 48
    [SYS_chdir] = sys_chdir,                    // 49
    //[SYS_fchmodat] = sys_fchmodat,              // 53
    //[SYS_fchownat] = sys_fchownat,              // 54
    //[SYS_fchown] = sys_fchown,                  // 55
    [SYS_openat] = sys_openat,                  // 56
    [SYS_close] = sys_close,                    // 57
    [SYS_pipe2] = sys_pipe2,                    // 59
    [SYS_getdents64] = sys_getdents64,            // 61
    [SYS_lseek] = (func)sys_lseek,              // 62
    [SYS_read] = (func)sys_read,                // 63
    [SYS_write] = (func)sys_write,              // 64
    //[SYS_readv] = (func)sys_readv,              // 65
    [SYS_writev] = (func)sys_writev,            // 66
    //[SYS_pread64] = sys_pread64,                // 67
    //[SYS_ppoll] = sys_ppoll,                    // 73
    [SYS_readlinkat] = (func)sys_readlinkat,    // 78
    //[SYS_newfstatat] = sys_fstatat,             // 79
    [SYS_fstat] = sys_fstat,                    // 80
    //[SYS_fsync] = sys_fsync,                    // 82
    //[SYS_fdatasync] = sys_fdatasync,            // 83
    //[SYS_utimensat] = sys_utimensat,            // 88
    [SYS_exit] = sys_exit,                      // 93
    // FIXME: exit_group should kill every thread in the current thread group.
    [SYS_exit_group] = sys_exit_group,          // 94
    //[SYS_set_tid_address] = sys_set_tid_address,  // 96 OK
    //[SYS_nanosleep] = sys_nanosleep,            // 101
    //[SYS_getitimer] = sys_getitimer,            // 102
    //[SYS_setitimer] = sys_setitimer,            // 103
    //[SYS_clock_settime] = sys_clock_settime,    // 112
    //[SYS_clock_gettime] = sys_clock_gettime,    // 113
    //[SYS_sched_getaffinity] = sys_sched_getaffinity, // 123
    //[SYS_sched_yield] = sys_yield,              // 124
    //[SYS_kill] = sys_kill,                      // 129
    //[SYS_tkill] = sys_tkill,                    // 130
    //[SYS_rt_sigsuspend] = sys_rt_sigsuspend,    // 133
    //[SYS_rt_sigaction] = sys_rt_sigaction,      // 134
    //[SYS_rt_sigprocmask] = sys_rt_sigprocmask,  // 135
    //[SYS_rt_sigpending] = sys_rt_sigpending,    // 136
    //[SYS_rt_sigreturn] = sys_rt_sigreturn,      // 139
    //[SYS_setregid] = sys_setregid,              // 143
    //[SYS_setgid] = sys_setgid,                  // 144
    //[SYS_setreuid] = sys_setreuid,              // 145
    //[SYS_setuid] = sys_setuid,                  // 146
    //[SYS_setresuid] = sys_setresuid,            // 147
    //[SYS_getresuid] = sys_getresuid,            // 148
    //[SYS_setresgid] = sys_setresgid,            // 149
    //[SYS_getresgid] = sys_getresgid,            // 150
    //[SYS_setfsuid] = sys_setfsuid,              // 151
    //[SYS_setfsgid] = sys_setfsgid,              // 152
    //[SYS_setpgid] = sys_setpgid,                // 154
    //[SYS_getpgid] = sys_getpgid,                // 155
    //[SYS_getgroups] = sys_getgroups,            // 158
    //[SYS_setgroups] = sys_setgroups,            // 159
    //[SYS_uname] = sys_uname,                    // 160
    //[SYS_umask] = (func)sys_umask,              // 166
    //[SYS_getpid] = sys_getpid,                  // 172
    //[SYS_getppid] = sys_getppid,                // 173
    //[SYS_getuid] = sys_getuid,                  // 174
    //[SYS_geteuid] = sys_geteuid,                // 175
    //[SYS_getgid] = sys_getgid,                  // 176
    //[SYS_getegid] = sys_getegid,                // 177
    //[SYS_gettid] = sys_gettid,                  // 178
    //[SYS_sysinfo] = sys_sysinfo,                // 179
    //[SYS_socket] = sys_socket,                  // 198
    //[SYS_bind] = sys_bind,                      // 200
    //[SYS_listen] = sys_listen,                  // 201
    //[SYS_accept] = sys_accept,                  // 202
    //[SYS_connect] = sys_connect,                // 203
    //[SYS_sendto] = sys_sendto,                  // 206
    //[SYS_recvfrom] = sys_recvfrom,              // 207
    [SYS_brk] = (func)sys_brk,                  // 214
    //[SYS_munmap] = sys_munmap,                  // 215
    //[SYS_mremap] = (func)sys_mremap,            // 216
    [SYS_clone] = sys_clone,                    // 220
    [SYS_execve] = sys_execve,                  // 221
    //[SYS_mmap] = (func)sys_mmap,                // 222
    //[SYS_fadvise64] = sys_fadvise64,            // 223
    //[SYS_mprotect] = sys_mprotect,              // 226
    //[SYS_msync] = sys_msync,                    // 227
    //[SYS_madvise] = sys_madvise,                // 233
    [SYS_wait4] = sys_wait4,                    // 260
    //[SYS_prlimit64] = sys_prlimit64,            // 261
    //[SYS_renameat2] = sys_renameat2,            // 276
    //[SYS_getrandom] = sys_getrandom,            // 278
    //[SYS_faccessat2] = sys_faccessat2,          // 439
    //[SYS_dns] = sys_dns,          // 900
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
    [SYS_renameat] = "sys_renameat",              // 38
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
    [SYS_pread64] = "sys_pread64",                // 67
    [SYS_ppoll] = "sys_ppoll",                    // 73
    [SYS_readlinkat] = "sys_readlinkat",          // 78
    [SYS_newfstatat] = "sys_fstatat",             // 79
    [SYS_fstat] = "sys_fstat",                    // 80
    [SYS_fstat] = "sys_fstat",                    // 80
    [SYS_fsync] = "sys_fsync",                    // 82
    [SYS_fdatasync] = "sys_fdatasync",            // 83
    [SYS_utimensat] = "sys_utimensat",            // 88
    [SYS_exit] = "sys_exit",                      // 93
    [SYS_exit_group] = "sys_exit_group",          // 94
    [SYS_set_tid_address] = "sys_gettid",         // 96
    [SYS_nanosleep] = "sys_nanosleep",            // 101
    [SYS_getitimer] = "sys_getitimer",            // 102
    [SYS_setitimer] = "sys_setitimer",            // 103
    [SYS_clock_settime] = "sys_clock_settime",    // 112
    [SYS_clock_gettime] = "sys_clock_gettime",    // 113
    [SYS_sched_getaffinity] = "sys_sched_getaffinity", // 123
    [SYS_sched_yield] = "sys_yield",              // 124
    [SYS_kill] = "sys_kill",                      // 129
    [SYS_tkill] = "sys_tkill",                    // 130
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
    [SYS_socket] = "sys_socket",                  // 198
    [SYS_bind] = "sys_bind",                      // 200
    [SYS_listen] = "sys_listen",                  // 201
    [SYS_accept] = "sys_accept",                  // 202
    [SYS_connect] = "sys_connect",                // 203
    [SYS_sendto] = "sys_sendto",                  // 206
    [SYS_recvfrom] = "sys_recvfrom",              // 207
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
    [SYS_dns] = "sys_dns",          // 900
};

#if 0
int syscall1(struct trapframe *tf)
{
    myproc()->tf = tf;
    int sysno = tf->x[8];

    if (sysno > 0 && sysno < ARRAY_SIZE(syscalls) && syscalls[sysno]) {
        if (sysno != SYS_sched_yield /* && sysno != SYS_rt_sigprocmask */ && myproc()->pid >= 8)
            trace("proc[%d] %s called", myproc()->pid, syscall_names[sysno]);
        return syscalls[sysno]();
    } else {
        debug_reg();
        char *name = syscall_names[sysno] ? syscall_names[sysno] : "unknown";
        panic("Unexpected syscall #%d (%s)\n", sysno, name);
        return 0;
    }
}
#endif
