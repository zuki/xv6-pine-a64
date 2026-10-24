#ifndef	INC_USER_SYS_H
#define	INC_USER_SYS_H

// for user threads (compiled as part of the kernel code)  to make syscalls
// user_sys.S
int call_sys_write(int fd, const void *buf, unsigned long n);
int call_sys_open(int dirfd, const char *buf, int flags, unsigned int mode);
int call_sys_mknod(int dirfd, const char *buf, unsigned int mode, unsigned long dev_t);
int call_sys_dup(int fd);
int call_sys_exec(const char *path, char *argv[], char *envp[]);
int call_sys_exit(int code);

#endif  /* INC_USER_SYS_H */
