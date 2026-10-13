#include <types.h>
#include <param.h>
#include <string.h>
#include <vfs.h>
#include <fs/vfile.h>
#include <fs/fatfs/fs.h>
#include <fs/fatfs/ff.h>
#include <fs/fatfs/diskio-ff.h>
#include <fs/nop.h>
#include <fs/bufcache.h>
#include <mm.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <rtc.h>

FATFS   fatfs;

struct {
  struct spinlock lock;
  struct fat_inode inode[NINODE];
} itable;

static int fat_init(void);
static int fat_mount(struct mount *mp, device_t dev, struct vnode *parent);
static int fat_unmount(struct mount *mp);
static int fat_sync(struct mount *mp);

struct mount_ops fat_mount_ops = {
    "fat",
    fat_init,
    fat_mount,
    fat_unmount,
    fat_sync,
};

#define ITOV(ip)    ((struct vnode *)ip)
#define VTOI(vp)    ((struct fat_inode *)vp->data)

static int fat_close(struct vfile *file);
static int fat_read(struct vfile *file, char *buffer, size_t size);
static off_t fat_seek(struct vfile *file, off_t offset, int whence);
static int fat_write(struct vfile *file, const char *buffer, size_t size);
static int fat_release(struct vnode *vnode);
static int fat_getdents(struct vfile *file, void *buffer, size_t size);

struct vfile_ops fat_file_ops = {
    nop_open,       // sys_openatで処理
    fat_close,
    fat_read,
    fat_write,
    nop_ioctl,
    nop_poll,
    fat_seek,
    nop_readdir,
    fat_getdents,
    nop_writeback,
    nop_chown
};

static int fat_release(struct vnode *vnode);

struct vnode_ops fat_vnode_ops = {
    &fat_file_ops,
    nop_create,     // sys_openatで処理
    nop_mknod,
    nop_lookup,
    nop_link,
    nop_symlink,
    nop_unlink,     // sys_unlinkで処理
    nop_rename,
    nop_truncate,
    nop_update,
    fat_release,
    nop_rmdir,      // sys_unlinkで処理
};

static int fat_init(void)
{
    int i = 0;

    initlock(&itable.lock, "fat_itable");
    for(i = 0; i < NINODE; i++) {
        initsleeplock(&itable.inode[i].lock, "fat_inode");
    }

    return 0;
}

static struct fat_inode* iget(device_t dev, ino_t ino);

static int fat_mount(struct mount *mp, device_t dev, struct vnode *parent)
{
    char devstr[32]; int ret;
    snprintf(devstr, 32, "%d:", SECONDDEV);
    if ((ret = f_mount(&fatfs, devstr, 1)) != FR_OK) {
        error("devstr %s ret %d", devstr, ret);
    } else {
        // これが必要。しないとf_chdir(..)のデフォルトがvol 0になる
        if ((ret = f_chdrive(devstr)) != FR_OK) {
            error("f_chdrive failed");
        }
        trace("f_chdrive ok");
    }

    mp->dev = dev;
    mp->super = NULL;
    struct fat_inode *ip = iget(dev, FAT_ROOTINO);
    if (ip == NULL) {
        return -ENOMEM;
    }
    mp->root_node = ITOV(ip);

    ip->vnode.mp = mp;
    ip->vnode.mode = S_IFDIR | 0755;
    ip->type = T_DIR_FAT;
    ip->valid = 1;

    return 0;
}

static int fat_unmount(struct mount *mp)
{
    vfs_release_vnode(mp->root_node);
    mp->root_node = NULL;
    return 0;
}

static int fat_sync(struct mount *mp)
{
    sync_bufcache();
    return 0;
}

static void ilock(struct fat_inode *ip)
{
    if (ip == 0 || ITOV(ip)->refcount < 0)
        panic("ilock");

    acquiresleep(&ip->lock);

    if (ip->valid == 0) {
        ip->type = T_FILE_FAT;   // default
        ip->vnode.nlink = 1;
        ip->valid = 1;
        ip->fatdir = NULL;
        ip->fatfp = NULL;
    }
}

static void iunlock(struct fat_inode *ip)
{
    if (ip == 0 || !holdingsleep(&ip->lock) || ITOV(ip)->refcount < 0)
        panic("iunlock");

    releasesleep(&ip->lock);
}

// ilock()されていること
static void iupdate(struct fat_inode *ip)
{
    trace("type: %d, mode: 0x%x, valid: %d", ip->type, ITOV(ip)->mode, ip->valid);
    if (ip->fatfp) {
        f_sync(ip->fatfp);
    }
}

// fat_releaseがすべて処理するので何もしない
static void iput(struct fat_inode *ip)
{
    trace("slock: ino=%d", ITOV(ip)->ino);

    acquiresleep(&ip->lock);
    if (ip->valid && ITOV(ip)->nlink == 0) {
        int r = ITOV(ip)->refcount;
        if (r == 1) {
            //itrunc(ip);
            ip->type = 0;
            iupdate(ip);
            ip->valid = 0;
        }
    }
    releasesleep(&ip->lock);

    // refcountはローカルではさわらない vfs_release_vnode()で行う
}

/* よくあるイディオム: unlockしてputする */
static void iunlockput(struct fat_inode *ip)
{
    iunlock(ip);
    iput(ip);
}

static struct fat_inode* iget(device_t dev, ino_t ino)
{
    struct fat_inode *ip, *empty;

    acquire(&itable.lock);

    // Is the inode already in the table?
    empty = 0;
    for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++){
        if (ITOV(ip)->refcount > 0 && ip->vnode.rdev == dev && ip->vnode.ino == ino) {
            ITOV(ip)->refcount++;
            release(&itable.lock);
            return ip;
        }
        if (empty == 0 && ITOV(ip)->refcount == 0)    // Remember empty slot.
            empty = ip;
    }

    // Recycle an inode entry.
    // fxl: only fill in basic info of the in-mem inode. wont read from disk, which is
    // done by ilock()
    if (empty == 0)
        panic("iget: no inodes");

    ip = empty;
    vfs_init_vnode(&ip->vnode, &fat_vnode_ops, NULL, 0, 1, 0, 0, dev, ino, 0, FSFAT, 0, 0, 0);
    ip->valid = 0;
    ip->vnode.data = ip;
    release(&itable.lock);

    return ip;
}

#if 0
// TODO
static void itrunc(struct fat_inode *ip) {
    return;
}
#endif

// http://www.cse.yorku.ca/~oz/hash.html
// fatpath: fatfsのネイティブの絶対パス, たとえば. "3:/myfile"
// ディレクトリ/ファイルのいずれにも適用
// TBD: 末尾の "/" の削除などのパスの正規化が必要?
static unsigned int fatpath_to_ino(const char *fatpath) {
    unsigned hash = 5381;
    int c;
    while ((c = *fatpath++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

// 単にiノードを作成するだけで、ファイルやディレクトリを開く必要はない。
// 単にf_chdir()などを実行するだけかもしれないからである。
//   fatpath: fatネイティブpath ("4:file.txt"など)
static struct fat_inode* namei_fat(char *fatpath) {
    return iget(FAT2MINOR, fatpath_to_ino(fatpath));
}

// 以下は FATFS固有のvnode操作関数とvfile操作関数

static int fat_close(struct vfile *file) {
    struct fat_inode *ip = (struct fat_inode *)file->vnode->data;

    ilock(ip);
    if (ip->fatfp) {
        f_close(ip->fatfp);
        kmfree(ip->fatfp);
    } else if (ip->fatdir) {
        f_closedir(ip->fatdir);
        kmfree(ip->fatdir);
    }
    iunlockput(ip);
    return 0;
}

extern int copyout(uint64_t *pgdir, void *va, void *p, size_t len);

static int fat_read(struct vfile *file, char *buffer, size_t size)
{
    int r = -EINVAL;

    struct fat_inode *ip = (struct fat_inode *)file->vnode->data;
    ilock(ip);
    trace("ino: %lu, type: %d", ITOV(ip)->ino, ip->type);

    if (ip->type != T_FILE_FAT && ip->type != T_DIR_FAT) {
        warn("wrong type: %d", ip->type);
        iunlock(ip);
        return -EINVAL;
    }
    if (ip->type == T_FILE_FAT) {
        char *buf = kmalloc(size);
        if (!buf) {
            error("no memory");
            return r;
        }
        unsigned n1;

        if ((r = f_read(ip->fatfp, buf, size, &n1)) == FR_OK) {
            trace("file read: %d", n1);
            memmove(buffer, buf, n1);
            file->offset += n1;
            r = (int)n1;
        } else {
            warn("f_read failed returns %d, size=%d", r, size);
        }
        kmfree(buf);
    } else {
        int sz = sizeof (FILINFO);
        if (size >= sz) {
            FILINFO fno;
            if (!ip->fatdir) {
                error("ip is dir but has not fatdir");
                return r;
            }
            if (f_readdir(ip->fatdir, &fno) == FR_OK) {
                if (fno.fname[0]) {
                    memmove(buffer, &fno, sz);
                    trace("readdir: %s", fno.fname);
                    r = sz;
                } else {
                    trace("no more entry");
                    r = 0; // no more dir entries
                }
            } else {
                warn("f_readdir failed");
            }
        }
    }
    iunlock(ip);
    trace("ok: %d", r);
    return r;
}

static off_t fat_seek(struct vfile *file, off_t offset, int whence)
{
    struct fat_inode *ip = (struct fat_inode *)file->vnode->data;

    ilock(ip);
    off_t size = ITOV(ip)->size;
    switch (whence) {
    case SEEK_SET:
        if (offset > size || offset < 0)
            goto bad;
        break;
    case SEEK_CUR:
        if (offset < 0 && -offset > file->offset)
            goto bad;
        offset = file->offset + offset;
        break;
    case SEEK_END:
        if (offset < 0 && -offset > size)
            goto bad;
        offset = size + offset;
        break;
    default:
        goto bad;
    }

    if (f_lseek(ip->fatfp, offset) != FR_OK)
        goto bad;
    file->offset = offset;
    iunlock(ip);
        return offset;

bad:
    warn("invalid offset: %d, size: %d", offset, size);
    iunlock(ip);
        return -EINVAL;
}

static int fat_write(struct vfile *file, const char *buffer, size_t size)
{
    int r = 0;
    struct fat_inode *ip = (struct fat_inode *)file->vnode->data;

    ilock(ip);
    trace("file: ino: %ld", file->vnode->ino);
    char *buf = kmalloc(size);
    if (!buf) {
        error("no memory");
        iunlock(ip);
        return -2;
    }
    unsigned n1;
    FRESULT fr = 9999;  // invalid
    memmove(buf, buffer, size);
    if ((fr=f_write(ip->fatfp, buf, size, &n1)) == FR_OK) {
        if (size != n1)
            warn("f_write ok but disk is full");
        file->offset += n1;
        r = (int)n1;
    } else {
        error("failed. f_write returns %d", fr);
        r = -EINVAL;
    }
    kmfree(buf);
    iunlock(ip);
    return r;
}

static int fat_release(struct vnode *vnode)
{
    trace("ino: %d, refcount: %d", vnode->ino, vnode->refcount);

    if (vnode->ino == 0 || vnode->refcount == 0) {
        struct fat_inode *ip = VTOI(vnode);
        ilock(ip);
        ip->type = 0;
        if (ip->fatfp) kmfree(ip->fatfp);
        if (ip->fatdir) kmfree(ip->fatdir);
        vnode->ino = 0;
        vnode->nlink = 0;
        iupdate(ip);
        iunlockput(ip);
    }
    return 0;
}

static int fat_getdents(struct vfile *file, void *buffer, size_t size)
{
    FILINFO info;
    FRESULT ret;

    int namelen, reclen, tlen = 0;
    off_t offset = 0;
    struct dirent64 de64;

    struct fat_inode *ip = VTOI(file->vnode);

    if (!ip->fatdir) {
        error("ip hasn't fatdir");
        return -ENOENT;
    }

//FR_OK, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_INVALID_OBJECT, FR_TIMEOUT, FR_NOT_ENOUGH_CORE
    while(1) {
        if ((ret = f_readdir(ip->fatdir, &info)) != FR_OK) {
            error("f_readdir failed: %d", ret);
            return tlen ? tlen : -EIO;
        }
        if (info.fname[0] == 0) {   // すべて読み込み済み
            return tlen ? tlen : 0;
        }
        if (info.fname[0] == '.')
            continue;               // ドットエントリは無視

        namelen = MIN(strlen(info.fname), DIRSIZE);
        reclen = (size_t)(&((struct dirent64 *)0)->d_name);
        reclen += namelen;
        reclen = (reclen + 0x7) & ~0x7;
        memset(&de64, 0, reclen+1); // null終端で+1

        // sizeまで詰めたらreturn
        if ((tlen + reclen) > size) {
            trace("break; tlen: %d, reclen: %d, size: %d", tlen, reclen, size);
            break;
        }

        de64.d_ino = -1;
        de64.d_off = offset;
        de64.d_reclen = reclen;
        de64.d_type = info.fattrib & AM_DIR ? DT_DIR : DT_REG;
        memmove(de64.d_name, info.fname, namelen);
        memmove(buffer + tlen, (char *)&de64, reclen);
        tlen += reclen;
        offset += namelen;
    }
    return tlen;
}

// 以下はvs_xxxを通さず、sys_xxxから直接呼び出す

static boolean redirect_fatpath(const char *path, char *fatpath,
    int *fat_rela, int *fat_abs);


// 成功したらfat_inodeを返す。
// path: fatfsネイティブの絶対パス, 例. "3:/myfile"
static struct fat_inode *open_path(const char *path, int omode) {
    struct fat_inode *ip = NULL;
    int flag = 0, isdir = 0;
    unsigned fsize = 0;         // ファイルサイズ
    FRESULT ret;
    FILINFO info;

    trace("path: %s omode: 0x%x", path, omode);

    if ((ret = f_stat(path, &info)) == FR_OK) { // file/dirは存在
        isdir = info.fattrib & AM_DIR;
        if (!isdir)
            fsize = info.fsize;

        // access rule check (more?)
#if 0
        if (isdir && omode != O_RDONLY) {
            debug("isdir but not O_RDONLY");
            return NULL;
        }
        if ((info.fattrib & AM_RDO) && (omode & 0x3) != O_RDONLY) {
            debug("fattrib is ro but omode is not O_RDONLY");
            return NULL;
        }
#endif
    }

    if ((omode & 0x3) == O_RDONLY)
        flag |= FA_READ;
    else if (omode & O_WRONLY || omode & O_RDWR)
        flag |= FA_WRITE;
    if (omode & O_CREAT)
        flag |= FA_CREATE_NEW;
    if (omode & O_TRUNC) {  // used in sh.c ">"
        flag |= FA_CREATE_ALWAYS;
    }
    // 仮のinoを取得する。itableのidexとして使用する
    // (XXX need a better way to generate ino..., maybe obj id in FILINFO?)
    // the problem: need to open the file/dir before iget()
    ip = iget(DEVFAT2, fatpath_to_ino(path));
    ilock(ip);
    if (isdir) {
        ip->type = T_DIR_FAT;
        if (!(ip->fatdir = kmalloc(sizeof(DIR)))) {
            debug("failed kmalloc for fatdir");
            iunlockput(ip);
            return NULL;
        }
        if ((ret = f_opendir(ip->fatdir, path)) != FR_OK) {
            warn("f_opendir '%s' failed with ret %d", path, ret);
            kmfree(ip->fatdir);
            iunlockput(ip);
            return NULL;
        }
        ip->fatdir->obj.attr |= AM_DIR;
    } else { // normal file
        ip->type = T_FILE_FAT;
        if (!(ip->fatfp = kmalloc(sizeof(FIL)))) {
            debug("failed kmalloc for fatfp");
            iunlockput(ip);
            return NULL;
        }
        if ((ret = f_open(ip->fatfp, path, flag)) != FR_OK) {
            warn("f_open '%s' failed with ret %d", path, ret);
            kmfree(ip->fatfp);
            iunlockput(ip);
            return NULL;
        }
        ITOV(ip)->size = (flag & FA_CREATE_ALWAYS) ? 0 : fsize;  // O_TRUNC or not?
    }
    iunlock(ip);
    trace("ino: %d, mode: 0x%x, dir: %d", ITOV(ip)->ino, ITOV(ip)->mode, isdir ? 1 : 0);
    return ip;
}

long fat_open(char *path, int flags, mode_t mode)
{
    int fd;
    struct vfile *file;
    int fat_rela = 0;
    int fat_abs = 0;
    char fatpath[MAXPATH], *pp;
    struct fat_inode *ip;
    struct proc *p = thisproc();
    BYTE attr = 0;

    if (!redirect_fatpath(path, fatpath, &fat_rela, &fat_abs))
        return -EACCES;

    pp = fat_abs ? fatpath : path;
    if ((ip = open_path(pp, flags)) == NULL) {
        error("fat_open failed");
        return -EACCES;
    }

    if (flags & O_DIRECTORY && !ip->fatdir) {
        debug("opendir but path %s isnot dir", pp);
        return -ENOTDIR;
    }

    if (ip->fatdir) {
        trace("fatdir: attr 0x%x", ip->fatdir->obj.attr);
        attr = ip->fatdir->obj.attr;
    } else if (ip->fatfp) {
        trace("fatfp: attr 0x%x", ip->fatfp->obj.attr);
        attr = ip->fatfp->obj.attr;
    }

    if (attr & AM_DIR) {
        ITOV(ip)->mode |= S_IFDIR;
    } else {
        ITOV(ip)->mode |= S_IFREG;
    }

    if ((fd = find_unused_fd(p->fd_table, 0)) < 0) {
        error("cound not get fd");
        return fd;
    }
    if ((file = alloc_file(ITOV(ip), flags)) == NULL) {
        error("count not get file");
        return -EMFILE;
    }

    file->vnode->data = ip;

    set_fd(p->fd_table, fd, file);
    if (flags & O_CLOEXEC)
        bit_add(p->fdflag, fd);
    trace("fd: %d, ip: ino: %d, mode: 0x%x, type: %d", fd, ITOV(ip)->ino, ITOV(ip)->mode, ip->type);
    return fd;
}

long fat_unlink(char *path)
{
    int fat_rela = 0, fat_abs = 0;
    char fatpath[MAXPATH], *pp;

    if (!redirect_fatpath(path, fatpath, &fat_rela, &fat_abs))
        return -EACCES;

    pp = fat_abs ? fatpath : path;
    return (f_unlink(pp) == FR_OK) ? 0 : -EACCES;
}

long fat_mkdir(char *path)
{
    int fat_rela = 0, fat_abs = 0;
    char fatpath[MAXPATH], *pp;

    if (!redirect_fatpath(path, fatpath, &fat_rela, &fat_abs))
        return -EINVAL;

    pp = fat_abs ? fatpath : path;
    return (f_mkdir(pp) == FR_OK) ? 0 : -EINVAL;
}

long fat_chdir(char *path)
{
    struct fat_inode *ip;
    int fat_rela = 0, fat_abs = 0;
    char fatpath[MAXPATH];

    if (!redirect_fatpath(path, fatpath, &fat_rela, &fat_abs))
        return -EINVAL;

    if (fat_abs) {
        if (f_chdir(fatpath) != FR_OK) {
            return -EINVAL;
        }
        trace("fat: f_chdir to %s", fatpath);
    } else if (fat_rela) {
        FRESULT res;
        if ((res = f_chdir(path)) != FR_OK) {
            error("failed %d",res);
            return -EINVAL;
        }
        if (f_getcwd(fatpath, MAXPATH) != FR_OK) {
            return -EINVAL;
        }
        trace("fat: f_chdir to %s (%s)", path, fatpath);
    } else {
        error("bad path: %s", path);
        return -ENOENT;
    }
    // fatpath: the fat native, abs path
    ip = namei_fat(fatpath);
    ilock(ip);
    ip->type = T_DIR_FAT;
    iunlock(ip);
        iput(thisproc()->cwd->data);  // fxl: iput b/c we are leaving this dir
    thisproc()->cwd = ITOV(ip);
    return 0;
}

long fat_stat(char *path, struct stat *st)
{
    struct stat sst;
    struct timespec ts;
    int fat_rela = 0, fat_abs = 0;
    char fatpath[MAXPATH], *pp;
    FILINFO info;
    FRESULT ret;

    if (!redirect_fatpath(path, fatpath, &fat_rela, &fat_abs))
        return -EACCES;

    pp = fat_abs ? fatpath : path;
    trace("pp: %s", pp);
    if ((ret = f_stat(pp, &info)) != FR_OK) {
        error("fat_open failed");
        return -EACCES;
    }

    sst.st_dev = (info.fattrib & AM_DIR) ? T_DIR_FAT : T_FILE_FAT;
    sst.st_ino = 0;
    sst.st_mode = (info.fattrib & AM_DIR) ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    sst.st_nlink = 1;
    sst.st_uid = 0;
    sst.st_gid = 0;
    sst.st_rdev = DEVFAT2;
    sst.st_size = info.fsize;
    trace("fdate: 0x%x, ftime: 0x%x", info.fdate, info.ftime);
    rtc_fattime_to_time((uint32_t)info.fdate, (uint32_t)info.ftime, &ts);
    trace("ts.tv_sec: 0x%x", ts.tv_sec);
    memmove(&sst.st_atime, &ts, sizeof(struct timespec));
    memmove(&sst.st_mtime, &ts, sizeof(struct timespec));
    memmove(&sst.st_ctime, &ts, sizeof(struct timespec));
    memmove(st, &sst, sizeof(struct stat));
// 0x5cca : 0101110011001010 0101110=0x2e 46+1980=2026 : 0110=0x6 : 01010=0x0a=10
// 5432109876543210
// 0x7074 : 0111000001110100 01110=0xe=14 : 000011=3 : 10100=0x14=20
    return 0;
}

static boolean in_fatfs(void)
{
    struct proc *p = thisproc();
    if (!p->cwd) return false;

    return get_fsname(p->cwd) == FSFAT;
}

static boolean in_fatmount(const char *path)
{
    return (strncmp(path, "/d/", 3) == 0);
}

boolean is_fatfs(const char *path) {
    if (in_fatfs())
        return true;
    return in_fatmount(path);
}

// path is like "/d/file.txt", whereas
// ffs expects a path name like: "2:file.txt" where 2 is the volume id (i.e.
// our dev id) used in f_mount()
// pathout must be preallocated
static int to_fatpath(const char *path, char *fatpath, int dev) {
    return snprintf(fatpath, MAXPATH, "%d:%s", dev, path+2/*skip*/);
}

// "path" が fatのパス を指している場合は trueを返す
// これは以下のいずれかである。
// 0. cwdがfatにあり、pathが相対パスである。fat_relaに1をセットする。fatpathはそのまま。
// 1. pathがfatマウントポイントからの絶対パスである。
//    この場合、fat nativeパスに変換してfatpathに保存し(fatpath!=nullの場合）
//    fat_absに1をセットする    ("/d/file.txt" => "4:file.txt")
// fatpath != nullの場合、その長さはMAXPATH以上でなければならない
// 制限: 相対パスはfatマウントポイントをまたぐ(go in/out)ことはできない
//  たとえば、 cd /d/; cd ../otherdir/ はできない
// 注: fat API (f_XXX) で相対パスを使用する際は f_chdrive() を使用して CurrVol を
// 明示的に設定する必要がある。そうしないと、ボリューム 0 が使用される。
static boolean redirect_fatpath(const char *path /*in*/, char *fatpath /*out*/,
    int *fat_rela /*out*/, int *fat_abs /*out*/) {
    int fa = 0;
    *fat_rela = *fat_abs = 0;

    if (in_fatfs())
        fa = 1;

    trace("redirect_fatpath: %s cwd is fat? %d", path, fa);

    if (fa && path[0] != '/') {
        *fat_rela = 1;
        return true;
    }

    if (in_fatmount(path)) { // an abs path via fatfs mount
        if (fatpath)
            to_fatpath(path, fatpath, SECONDDEV);
        *fat_abs = 1;
        return true;
    }

    return false;
}

#if !FF_FS_READONLY && !FF_FS_NORTC
#include <linux/time.h>
#include <rtc.h>

DWORD get_fattime (void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
        return ((DWORD)(FF_NORTC_YEAR - 1980) << 25 | (DWORD)FF_NORTC_MON << 21 | (DWORD)FF_NORTC_MDAY << 16);
    }
    return (DWORD)rtc_time_to_fattime(&now);
}
#endif
