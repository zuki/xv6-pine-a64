#ifndef INC_SYSCALL1_H
#define INC_SYSCALL1_H

#include <types.h>
#include <trap.h>
#include <file.h>
#include <linux/syscall.h>
#include <linux/fcntl.h>

#define MAXARG      32
#define MAXENV      64

typedef long (*func)();

// kern/syscall.c
int in_user(void *s, size_t n);
long argstr(int n, char **pp);
long argint(int n, int *ip);
long argu64(int n, uint64_t * ip);
long argfd(int n, int *pfd, struct file **pf);
long argptr(int n, char **pp, size_t size);
long fetchstr(uint64_t addr, char **pp);
long sys_clock_gettime(void);
long sys_sched_getaffinity(void);
long sys_prlimit64(void);
long sys_sysinfo(void);
long sys_nanosleep(void);
long sys_getrandom(void);
long sys_uname(void);
long sys_clock_settime(void);
long sys_ppoll(void);
long sys_madvise(void);
long syscall1(struct trapframe *tf);

// kern/sysproc.c
size_t sys_brk(void);
long sys_yield(void);
long sys_clone(void);
long sys_wait4(void);
long sys_exit(void);
long sys_exit_group(void);
long sys_rt_sigprocmask(void);
long sys_rt_sigsuspend(void);
long sys_rt_sigaction(void);
long sys_rt_sigpending(void);
long sys_rt_sigreturn(void);
long sys_kill(void);
long sys_tkill(void);
mode_t sys_umask(void);
long sys_getpgid(void);
long sys_setpgid(void);
long sys_setregid(void);
long sys_setgid(void);
long sys_setreuid(void);
long sys_setuid(void);
long sys_setresuid(void);
long sys_setresgid(void);
long sys_getresuid(void);
long sys_getresgid(void);
long sys_getuid(void);
long sys_geteuid(void);
long sys_getgid(void);
long sys_getegid(void);
long sys_setfsuid(void);
long sys_setfsgid(void);
long sys_getgroups(void);
long sys_setgroups(void);
long sys_gettid(void);
long sys_getpid(void);
long sys_getppid(void);
long sys_set_tid_address(void);
void *sys_mmap(void);
long sys_munmap(void);
long sys_msync(void);
void *sys_mremap(void);
long sys_mprotect(void);
long sys_madvise(void);
long sys_getitimer(void);
long sys_setitimer(void);

// kern/sysfile.c
long sys_execve(void);
long sys_dup(void);
long sys_dup3(void);
long sys_fcntl(void);
long sys_ioctl(void);
long sys_pipe2(void);
long sys_fstat(void);
long sys_fstatat(void);
ssize_t sys_read(void);
ssize_t sys_readv(void);
ssize_t sys_write(void);
ssize_t sys_writev(void);
off_t sys_lseek(void);
long sys_fsync(void);
long sys_fdatasync(void);
long sys_close(void);
long sys_openat(void);
long sys_getdents64(void);
long sys_mkdirat(void);
long sys_mknodat(void);
long sys_unlinkat(void);
long sys_symlinkat(void);
ssize_t sys_readlinkat(void);
long sys_linkat(void);
long sys_chdir(void);
long sys_faccessat(void);
long sys_faccessat2(void);
long sys_fchmodat(void);
long sys_fchownat(void);
long sys_fchown(void);
long sys_umount2(void);
long sys_mount(void);
long sys_renameat(void);
long sys_renameat2(void);
void *sys_getcwd(void);
long sys_fadvise64(void);
long sys_utimensat(void);
long sys_pread64(void);
//int dirunlink(struct inode *, char *, uint32_t);
//int direntlookup(struct inode *, int, struct dirent *);

// kern/exec.c
//long do_execve(char *, int, int, char **, char **);

// syssocket.c
long sys_ioctl(void);
long sys_socket(void);
long sys_connect(void);
long sys_bind(void);
long sys_listen(void);
long sys_accept(void);
long sys_recv(void);
long sys_send(void);
long sys_recvfrom(void);
long sys_sendto(void);
long sys_dns(void);

#endif
