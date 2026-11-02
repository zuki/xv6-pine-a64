#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>

#include "types.h"
#include "files.h"

// this file should be compiled with normal gcc...
#define stat xv6_stat           // avoid clash with host struct stat
#define sleep xv6_sleep
#include "usr_fs.h"

#define NINODES 200

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif


// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE / (BLKSIZE * 8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;                      // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;                    // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BLKSIZE];
uint32_t freeinode = 1;
uint32_t freeblock;


void balloc(int);
void wsect(uint32_t, void *);
void winode(uint32_t, struct dinode *);
void rinode(uint32_t inum, struct dinode *ip);
void rsect(uint32_t sec, void *buf);
uint32_t ialloc(uint16_t type, uid_t uid, gid_t gid, mode_t mode);
void iappend(uint32_t inum, void *p, int n);

void make_dirent(uint32_t inum, uint16_t type, uint32_t parent, char *name);
uint32_t make_dir(uint32_t parent, char *name, uid_t uid, gid_t gid, mode_t mode);
uint32_t make_file(uint32_t parent, char *name, uid_t uid, gid_t gid, mode_t mode);
uint32_t make_dev(uint32_t parent, char *name, int major, int minor, uid_t uid, gid_t gid, mode_t mode);
void copy_file(int start, int argc, char *files[], uint32_t parent, uid_t uid, gid_t gid, mode_t mode);

// convert to little-endian byte order
uint16_t
xshort(uint16_t x)
{
    uint16_t y;
    uint8_t *a = (uint8_t *) & y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

uint32_t
xint(uint32_t x)
{
    uint32_t y;
    uint8_t *a = (uint8_t *) & y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

uint64_t xlong(uint64_t x)
{
    uint64_t y;
    uint8_t *a = (uint8_t*)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    a[4] = x >> 32;
    a[5] = x >> 40;
    a[6] = x >> 48;
    a[7] = x >> 56;
    return y;
}

int
main(int argc, char *argv[])
{
    int i, cc, fd;
    uint32_t rootino, inum, off, binino, devino, procino, libino;
    struct dirent de;
    char buf[BLKSIZE];
    struct dinode din;

    static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs fs.img files...\n");
        exit(1);
    }

    assert((BLKSIZE % sizeof(struct dinode)) == 0);
    assert((BLKSIZE % sizeof(struct dirent)) == 0);

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0) {
        perror(argv[1]);
        exit(1);
    }

    // 1 fs block = 1 disk sector
    nmeta = 2 + nlog + ninodeblocks + nbitmap;
    nblocks = FSSIZE - nmeta;
    sb.magic = FSMAGIC;
    sb.size = xint(FSSIZE);
    sb.nblocks = xint(nblocks);
    sb.ninodes = xint(NINODES);
    sb.nlog = xint(nlog);
    sb.logstart = xint(2);
    sb.inodestart = xint(2 + nlog);
    sb.bmapstart = xint(2 + nlog + ninodeblocks);

    printf
        ("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) blocks %d total %d\n",
         nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

    freeblock = nmeta;          // the first free block that we can allocate

    for (i = 0; i < FSSIZE; i++)
        wsect(i, zeroes);

    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

    // create /
    rootino = ialloc(T_DIR, 0, 0, S_IFDIR|0775);
    assert(rootino == V6_ROOTINO);
    make_dirent(rootino, T_DIR, rootino, ".");
    make_dirent(rootino, T_DIR, rootino, "..");

    // create /bin
    binino = make_dir(rootino, "bin", 0, 0, S_IFDIR|0775);

    // Create /dev
    devino = make_dir(rootino, "dev", 0, 0, S_IFDIR|0775);

    // Create /proc
    procino = make_dir(rootino, "proc", 0, 0, S_IFDIR|0755);

    // Create /lib
    libino = make_dir(rootino, "lib", 0, 0, S_IFDIR|0777);

    // Create /dev/console
    make_dev(devino, "console", 1, 0, 0, 0, S_IFCHR|0666);

    // copy /lib/*
    //copy_file(0, nelms(lib_files), lib_files, libino, 0, 0, S_IFREG|0755);

    // copy /*
    copy_file(2, 3, argv, rootino, 0, 0, S_IFREG|0755);
    copy_file(3, argc, argv, binino, 0, 0, S_IFREG|0755);

    // fix size of root inode dir
    rinode(rootino, &din);
    off = xint(din.size);
    off = (((off - 1) / BLKSIZE) + 1) * BLKSIZE;
    din.size = xint(off);
    winode(rootino, &din);

    balloc(freeblock);

    exit(0);
}

void
make_dirent(uint32_t inum, uint16_t type, uint32_t parent, char *name)
{
    struct dirent de;

    bzero(&de, sizeof(de));
    de.inum = xint(inum);
    de.type = xshort(type);
    strncpy(de.name, name, DIRSIZ);
    //printf("DIRENT: inum=%d, name='%s' to PARNET[%d]\n", de.inum, de.name, parent);
    iappend(parent, &de, sizeof(de));
}

uint32_t
make_dir(uint32_t parent, char *name, uid_t uid, gid_t gid, mode_t mode)
{
    // Create parent/name
    uint32_t inum = ialloc(T_DIR, uid, gid, mode);
    make_dirent(inum, T_DIR, parent, name);

    // Create parent/name/.
    make_dirent(inum, T_DIR, inum, ".");

    // Create parent/name/..
    make_dirent(parent, T_DIR, inum, "..");

    return inum;
}

uint32_t make_dev(uint32_t parent, char *name, int major, int minor, uid_t uid, gid_t gid, mode_t mode)
{
    struct dinode din;

    uint32_t inum = ialloc(T_DEVICE, uid, gid, mode);
    make_dirent(inum, T_DEVICE, parent, name);
    rinode(inum, &din);
    din.devno = xint((device_t)makedev(major, minor));
    winode(inum, &din);
    return inum;
}

uint32_t
make_file(uint32_t parent, char *name, uid_t uid, gid_t gid, mode_t mode)
{
    uint32_t inum = ialloc(T_FILE, uid, gid, mode);
    make_dirent(inum, T_FILE, parent, name);
    return inum;
}

void
copy_file(int start, int argc, char *files[], uint32_t parent, uid_t uid, gid_t gid, mode_t mode)
{
    int fd, cc;
    uint32_t inum;
    char buf[BLKSIZE];

    for (int i = start; i < argc; i++) {
        char *path = files[i];
        int j = 0;
        for (; *files[i]; files[i]++) {
            if (*files[i] == '/') j = -1;
            j++;
        }
        files[i] -= j;
        printf("input: '%s' -> '%s'\n", path, files[i]);

        assert(index(files[i], '/') == 0);

        if ((fd = open(path, 0)) < 0) {
            perror(files[i]);
            exit(1);
        }

        inum = make_file(parent, files[i], uid, gid, mode);
        while ((cc = read(fd, buf, sizeof(buf))) > 0)
            iappend(inum, buf, cc);
        close(fd);
    }
}

void
wsect(uint32_t sec, void *buf)
{
    if (lseek(fsfd, sec * BLKSIZE, 0) != sec * BLKSIZE) {
        perror("lseek");
        exit(1);
    }
    if (write(fsfd, buf, BLKSIZE) != BLKSIZE) {
        perror("write");
        exit(1);
    }
}

void
winode(uint32_t inum, struct dinode *ip)
{
    char buf[BLKSIZE];
    uint32_t bn;
    struct dinode *dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *dip = *ip;
    wsect(bn, buf);
}

void
rinode(uint32_t inum, struct dinode *ip)
{
    char buf[BLKSIZE];
    uint32_t bn;
    struct dinode *dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *ip = *dip;
}

void
rsect(uint32_t sec, void *buf)
{
    if (lseek(fsfd, sec * BLKSIZE, 0) != sec * BLKSIZE) {
        perror("lseek");
        exit(1);
    }
    if (read(fsfd, buf, BLKSIZE) != BLKSIZE) {
        perror("read");
        exit(1);
    }
}

uint32_t
ialloc(uint16_t type, uid_t uid, gid_t gid, mode_t mode)
{
    uint32_t inum = freeinode++;
    struct dinode din;
    struct timespec ts1, ts2;

    clock_gettime(CLOCK_REALTIME, &ts1);
    ts2.tv_sec = xlong(ts1.tv_sec);
    ts2.tv_nsec = xlong(ts1.tv_nsec);
    //printf("inum[%d] ts: sec %ld, nsec %ld\n", inum, ts2.tv_sec, ts2.tv_nsec);
    bzero(&din, sizeof(din));
    din.type  = xshort(type);
    din.nlink = xshort(1);
    din.devno = xint(DEVV6);
    din.size  = xint(0);
    din.mode  = xint(mode);
    din.uid   = xint(uid);
    din.gid   = xint(gid);
    din.atime = din.mtime = din.ctime = ts2;
    winode(inum, &din);
    return inum;
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
balloc(int used)
{
    uint8_t buf[BLKSIZE];
    int i, j, k;

    printf("balloc: first %d blocks have been allocated\n", used);
    assert(used < nbitmap * BLKSIZE * 8);

    int used_blk = (used - 1) / (BLKSIZE * 8) + 1;
    for (j = 0; j < used_blk; j++) {
        bzero(buf, BLKSIZE);
        k = min(BLKSIZE * 8, used - j * BLKSIZE * 8);
        for (i = 0; i < k; i++) {
            buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
        }
        wsect(sb.bmapstart + j, buf);
    }

    printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
    wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(uint32_t inum, void *xp, int n)
{
    char *p = (char *)xp;
    uint32_t fbn, off, n1;
    struct dinode din;
    char buf[BLKSIZE];
    uint32_t indirect[NINDIRECT];
    uint32_t indirect2[NINDIRECT];
    uint32_t x, idx1, idx2;

    rinode(inum, &din);
    off = xint(din.size);
    // printf("append inum %d at off %d sz %d\n", inum, off, n);
    while (n > 0) {
        fbn = off / BLKSIZE;
        assert(fbn < MAXFILE);
        if (fbn < NDIRECT) {
            if (xint(din.addrs[fbn]) == 0) {
                din.addrs[fbn] = xint(freeblock++);
            }
            x = xint(din.addrs[fbn]);
        } else if (fbn < (NDIRECT + NINDIRECT)) {
            if (xint(din.addrs[NDIRECT]) == 0) {
                din.addrs[NDIRECT] = xint(freeblock++);
            }
            rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
            idx1 = fbn - NDIRECT;
            if (indirect[idx1] == 0) {
                indirect[idx1] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
            }
            x = xint(indirect[idx1]);
        } else if (fbn < (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)) {
            if (xint(din.addrs[NDIRECT + 1]) == 0) {
                din.addrs[NDIRECT + 1] = xint(freeblock++);
            }
            rsect(xint(din.addrs[NDIRECT + 1]), (char *)indirect);
            idx1 = (fbn - NDIRECT - NINDIRECT) / NINDIRECT;
            idx2 = (fbn - NDIRECT - NINDIRECT) % NINDIRECT;
            if (xint(indirect[idx1]) == 0) {
                indirect[idx1] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT+1]), (char *)indirect);
            }
            rsect(xint(indirect[idx1]), (char *)indirect2);
            if (indirect2[idx2] == 0) {
                indirect2[idx2] = xint(freeblock++);
                wsect(xint(indirect[idx1]), (char *)indirect2);
            }
            x = xint(indirect2[idx2]);
        } else {
            printf("file is too big: fbr=%d\n", fbn);
            exit(1);
        }

        n1 = min(n, (fbn + 1) * BLKSIZE - off);
        rsect(x, buf);
        bcopy(p, buf + off - (fbn * BLKSIZE), n1);
        wsect(x, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = xint(off);
    winode(inum, &din);
}
