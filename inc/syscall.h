#ifndef INC_SYSCALL1_H
#define INC_SYSCALL1_H

#include <types.h>
#include <fs.h>
#include <linux/syscall.h>
#include <linux/fcntl.h>

#define MAXARG      32
#define MAXENV      64

typedef int (*func)();

// kern/syscall.c
int in_user(uint64_t addr, size_t n);
int argstr(uint64_t addr, char *buf, int max);
int argptr(uint64_t addr, char *pp, size_t size);

#if 0
int sys_clock_gettime(void);
int sys_sched_getaffinity(void);
int sys_prlimit64(void);
int sys_sysinfo(void);
int sys_nanosleep(void);
int sys_getrandom(void);
int sys_uname(void);
int sys_clock_settime(void);
int sys_ppoll(void);
int sys_madvise(void);
#endif

// kern/sysproc.c
size_t sys_brk(uint64_t addr);
int sys_clone(uint64_t flags, uint64_t stack, uint64_t ptid, uint64_t tls, uint64_t ctid);
int sys_exit_group(int status);
int sys_exit(int status);
int sys_wait4(int pid, uint64_t statusp, int options, uint64_t rusagep);

int sys_rt_sigprocmask(void);
int sys_rt_sigsuspend(void);
int sys_rt_sigaction(void);
int sys_rt_sigpending(void);
int sys_rt_sigreturn(void);
int sys_kill(void);
int sys_tkill(void);
mode_t sys_umask(void);
int sys_getpgid(void);
int sys_setpgid(void);
int sys_setregid(void);
int sys_setgid(void);
int sys_setreuid(void);
int sys_setuid(void);
int sys_setresuid(void);
int sys_setresgid(void);
int sys_getresuid(void);
int sys_getresgid(void);
int sys_getuid(void);
int sys_geteuid(void);
int sys_getgid(void);
int sys_getegid(void);
int sys_setfsuid(void);
int sys_setfsgid(void);
int sys_getgroups(void);
int sys_setgroups(void);
int sys_gettid(void);
int sys_getpid(void);
int sys_getppid(void);
int sys_set_tid_address(void);
void *sys_mmap(void);
int sys_munmap(void);
int sys_msync(void);
void *sys_mremap(void);
int sys_mprotect(void);
int sys_madvise(void);
int sys_getitimer(void);
int sys_setitimer(void);

// kern/sysfile.c
int sys_dup(int fd);
int sys_dup3(int fd1, int fd2, int flags);
ssize_t sys_read(int fd, uint64_t p, size_t count);
ssize_t sys_write(int fd, uint64_t p, size_t count);
ssize_t sys_readv(int fd, uint64_t iovp, int iovcnt);
ssize_t sys_writev(int fd, uint64_t iovp, int iovcnt);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_close(int fd);
int sys_fstat(int fd, uint64_t st);
int sys_linkat(int ofd, uint64_t opathp,
    int nfd, uint64_t npathp, int flags);
ssize_t sys_symlinkat(uint64_t tpathp, int nfd, uint64_t npathp);
int sys_unlinkat(int fd, uint64_t pathp, int flags);
ssize_t sys_readlinkat(int fd, uint64_t pathp, uint64_t bufp, size_t bufsize);
int sys_renameat(int ofd, uint64_t opathp, int nfd, uint64_t npathp);
int sys_renameat2(int ofd, uint64_t opathp,
    int nfd, uint64_t npathp, int flags);
int sys_openat(int fd, uint64_t pathp, int flags, mode_t mode);
int sys_mkdirat(int fd, uint64_t pathp, mode_t mode);
int sys_mknodat(int fd, uint64_t pathp, mode_t mode, dev_t dev);
int sys_chdir(uint64_t pathp);
int sys_execve(uint64_t pathp, uint64_t argvp, uint64_t envpp);
int sys_pipe2(uint64_t pipefdp, int flags);
int sys_ioctl(int fd, uint64_t req, uint64_t argp);
int sys_fcntl(int fd, int cmd, int args);
int sys_getdents64(int fd, uint64_t dirp, size_t count);

int sys_fstatat(void);
int sys_fsync(void);
int sys_fdatasync(void);
int sys_faccessat(void);
int sys_faccessat2(void);
int sys_fchmodat(void);
int sys_fchownat(void);
int sys_fchown(void);
int sys_umount2(void);
int sys_mount(void);
void *sys_getcwd(void);
int sys_fadvise64(void);
int sys_utimensat(void);
int sys_pread64(void);
//int dirunlink(struct inode *, char *, uint32_t);
//int direntlookup(struct inode *, int, struct dirent *);

// syssocket.c
int sys_socket(void);
int sys_connect(void);
int sys_bind(void);
int sys_listen(void);
int sys_accept(void);
int sys_recv(void);
int sys_send(void);
int sys_recvfrom(void);
int sys_sendto(void);
int sys_dns(void);

#endif
