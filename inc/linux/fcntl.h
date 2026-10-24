#ifndef INC_LINUX_FCNTL_H
#define INC_LINUX_FCNTL_H

#define O_SEARCH        O_PATH
#define O_EXEC          O_PATH
#define O_TTY_INIT      0

#define O_ACCMODE      (0x000003|O_SEARCH)
#define O_RDONLY        0x000000
#define O_WRONLY        0x000001
#define O_RDWR          0x000002

#define O_CREAT         0x000040
#define O_EXCL          0x000080
#define O_NOCTTY        0x000100
#define O_TRUNC         0x000200
#define O_APPEND        0x000400
#define O_NONBLOCK      0x000800
#define O_DSYNC         0x001000
#define O_ASYNC         0x002000
#define O_DIRECTORY     0x004000
#define O_NOFOLLOW      0x008000
#define O_DIRECT        0x010000
#define O_LARGEFILE     0x020000
#define O_NOATIME       0x040000
#define O_CLOEXEC       0x080000
#define O_SYNC          0x101000
#define O_RSYNC         0x101000
#define O_PATH          0x200000
#define O_TMPFILE       0x404000
#define O_NDELAY O_NONBLOCK

#define F_OK            0
#define R_OK            4
#define W_OK            2
#define X_OK            1
#define F_ULOCK         0
#define F_LOCK          1
#define F_TLOCK         2
#define F_TEST          3

#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_GETLK         5
#define F_SETLK         6
#define F_SETLKW        7
#define F_SETOWN        8
#define F_GETOWN        9
#define F_SETSIG        10
#define F_GETSIG        11

#define F_SETOWN_EX     15
#define F_GETOWN_EX     16

#define F_GETOWNER_UIDS 17

#define FD_CLOEXEC      1

#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_EACCESS          0x200
#define AT_NO_AUTOMOUNT     0x800
#define AT_EMPTY_PATH       0x1000
#define AT_STATX_SYNC_TYPE  0x6000
#define AT_STATX_SYNC_AS_STAT   0x0000
#define AT_STATX_FORCE_SYNC 0x2000
#define AT_STATX_DONT_SYNC  0x4000
#define AT_RECURSIVE        0x8000

#define POSIX_FADV_NORMAL       0
#define POSIX_FADV_RANDOM       1
#define POSIX_FADV_SEQUENTIAL   2
#define POSIX_FADV_WILLNEED     3
#ifndef POSIX_FADV_DONTNEED
#define POSIX_FADV_DONTNEED     4
#define POSIX_FADV_NOREUSE      5
#endif

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#define CLONE_VM        0x00000100      // linux/sched.h
#define CLONE_FS        0x00000200
#define CLONE_FILES     0x00000400
#define CLONE_SIGHAND   0x00000800
#define CLONE_PARENT    0x00008000
#define CLONE_THREAD    0x00010000


// デバイスメジャー番号
// modeled after: https://github.com/NJU-ProjectN/navy-apps/blob/master/README.md
// TODO: make procfs a seperate file type (not just a major dev number...)
enum {
  BLOCK = 0,        // ファイル・システム
  CONSOLE,          // /dev/console
  DEVNULL,          // /dev/null
  DEVZERO,          // /dev/zero
  PROCFS_CPUINFO,
  PROCFS_MEMINFO,
  NDEV              // max
};

// procfs, devfs
struct proc_dev_info {
  char type;
#define TYPE_DEVFS      0
#define TYPE_PROCFS     1
  int major;
  const char * path; // 0 no specific path
};

// procfs/devfs lookup table. define as macro here so that it can be included
// in both user (sh creates them) & kernel. the macro is only instantiated in specific files
// as needed.
#define PROC_DEV_TABLE  \
static struct proc_dev_info pdi[] =            \
{                                       \
    {.type = TYPE_DEVFS, .major = CONSOLE, .path = "/dev/console"}, \
    {.type = TYPE_DEVFS, .major = DEVNULL, .path = "/dev/null"}, \
    {.type = TYPE_DEVFS, .major = DEVZERO, .path = "/dev/zero"}, \
    {.type = TYPE_PROCFS, .major = PROCFS_CPUINFO, .path = "/proc/cpuinfo"},   \
    {.type = TYPE_PROCFS, .major = PROCFS_MEMINFO, .path = "/proc/meminfo"},   \
};

// max # of args passed to the kernel, via writing to a procfs
#define PROCFS_MAX_ARGS   8

#endif
