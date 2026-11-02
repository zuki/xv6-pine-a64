#include <types.h>
#include <cachepage.h>
#include <console.h>
#include <linux/errno.h>
#include <mmu.h>
#include <mm.h>
#include <list.h>
#include <sleeplock.h>
#include <spinlock.h>
#include <string.h>
#include <file.h>
#include <proc.h>
#include <linux/fcntl.h>

struct {
    struct cachepage pages[NPAGECACHE];
    int count;
    struct spinlock lock;
} cachepages;

/* cachepage機能を初期化する */
void cachepage_init(void)
{
    initlock(&cachepages.lock, "cachepage");

    acquire(&cachepages.lock);
    for (int i = 0; i < NPAGECACHE; i++) {
        cachepages.pages[i].page = kalloc(1);
        if (!cachepages.pages[i].page) {
            for (int j = 0; j < i; j++)
                kfree(cachepages.pages[j].page);
            error("memory exhausted: i = %d", i);
            release(&cachepages.lock);
            return;
        }
        initsleeplock(&cachepages.pages[i].lock, "cachepages.page");
    }
    cachepages.count = 0;
    release(&cachepages.lock);
    info("cachepage_init ok");
}

/* dev/ino/offsetを含むcachepageを探す.
 * cachepage.lockを保持していなければならない
 */
static struct cachepage *find_cachepage(device_t dev, ino_t ino, off_t offset)
{
    trace("called: dev: 0x%x, ino: %lld, offset: %lld", dev, ino, offset);
    if (!cachepages.lock.locked)
        panic("not locked");

    struct cachepage *page;
    for (int i = 0; i < NPAGECACHE; i++) {
        page = &cachepages.pages[i];
        if (page->dev == dev && page->ino == ino && page->offset == offset) {
            trace("found: pages[%d].page: 0x%llx", i, page);
            return page;
        }
    }
    trace("not found");
    return 0;
}

struct cachepage *get_cachepage(struct file *file, off_t offset)
{
    trace("called: file: %d, offset: 0x%x", file->ip->inum, offset);
    int err;
    device_t devno = file->ip->devno;
    ino_t ino = file->ip->inum;
    trace("before align: offset: 0x%x", offset);
    offset -= (offset % PGSIZE);  // ページアライン
    trace("after  align: offset: 0x%x", offset);

    acquire(&cachepages.lock);
    // 該当のページがキャッシュに存在するか?
    struct cachepage *res = find_cachepage(devno, ino, offset);
    // あれば返す
    if (res) {
        //hexdump(res, sizeof(struct cachepage), "found cpage");
        release(&cachepages.lock);
        acquiresleep(&res->lock);
        res->ref_count++;
        trace("hit: res=0x%llx", res);
        return res;
    }
    // なければcachepageを作成してファイルから読み込んでページを返す
    struct cachepage *cpage = &cachepages.pages[cachepages.count++];
    // キャッシュページはリンクバッファ
    if (cachepages.count == NPAGECACHE) {
        cachepages.count = 0;
    }
    release(&cachepages.lock);
    acquiresleep(&cpage->lock);
    memset(cpage->page, 0, PGSIZE);

    // file->offをセットする
    off_t offset_back = file->off;
    if ((err = filelseek(file, offset, SEEK_SET)) < 0) {
        error("seek error");
        goto err1;
    }
    //if (offset_back != file->off) debug("file offset: 0x%x -> 0x%x", offset_back, file->off);
    int n = fileread(file, (uint64_t)cpage->page, PGSIZE, 0);
    if (n < 0) {
        error("get_cachepage readi failed: n=%d, offset=%ld, size=%d",
            n, offset, PGSIZE);
        goto err1;
    }
    // cachepageへの読み込みによりf->offsetが+PGSIZEされるので戻す
    if (offset_back != file->off) file->off = offset_back;

    cpage->dev = file->ip->devno;
    cpage->ino = file->ip->inum;
    cpage->offset = offset;
    cpage->ref_count = 1;
    trace("alloc new cachepage[%d]: 0x%llx, dev: 0x%x, ino=%d, offset=0x%llx, read_bytes: 0x%x", cachepages.count-1, cpage, cpage->dev, cpage->ino, cpage->offset,n);
    return cpage;

err1:
    releasesleep(&cpage->lock);
    return NULL;
}

/* fileオフセットoffsetを含むcachepageのオフセットdest_offsetから
 * sizeバイトのデータをdestにコピー
 */
long copy_cachepage(struct file *file, off_t offset, char *dest, size_t size, off_t dest_offset)
{
    trace("called: dev: 0x%x, ino=%d, offset=0x%llx, dest=0x%p, size=0x%x, dest_offset=0x%llx",
            file->ip->devno, file->ip->inum, offset, dest, size, dest_offset);
    struct cachepage *cpage = get_cachepage(file, offset);
    if (cpage == NULL) {
        warn("get_cachepage failed");
        return -ENOMEM;
    }

    if (!holdingsleep(&cpage->lock))
        panic("not holding sleeqlock");

    trace("memmove from %p to %p with 0x%x bytes",
        cpage->page + dest_offset, dest, size);
    memmove(dest, cpage->page + dest_offset, size);
    releasesleep(&cpage->lock);
    return 0;
}

/* fileのオフセットoffsetからsizeバイトのデータをdestにコピー */
long copy_cachepages(struct file *file, char *dest, size_t size, off_t offset)
{
    trace("called: dev: 0x%x, ino: %d, dest=0x%llx, size=0x%x, offset=0x%llx",
            file->ip->devno, file->ip->inum, dest, size, offset);
    char *addr = dest;
    off_t ioff = offset < PGSIZE ? 0 : offset & ~(PGSIZE - 1);
    off_t doff = offset & (uint64_t)(PGSIZE - 1);
    uint64_t sz = (doff + size) > PGSIZE ? PGSIZE - doff : size;
    int npages = (size + doff + PGSIZE - 1) / PGSIZE;
    trace("copy %d pages", npages);

    long error;
    for (int i = 0; i < npages; i++) {
        if ((error = copy_cachepage(file, ioff, addr, sz, doff)) < 0)
            return error;
        ioff += PGSIZE;
        addr += sz;
        size -= sz;
        sz = size > PGSIZE ? PGSIZE : size;
        doff = 0;
    }
    return 0;
}

/* dev/ino/offsetに対応するcachepageをaddrのデータでsizeバイト更新する */
void update_cachepage(device_t dev, ino_t ino, off_t offset, char *addr, size_t size)
{
    trace("called: dev: 0x%x, ino: %d, offset=0x%x, addr=0x%p, size=0x%x", dev, ino, offset, addr, size);

    off_t alligned_offset = offset - (offset % PGSIZE);
    off_t start_addr = offset % PGSIZE;
    trace("  - aligned_offset=0x%d", alligned_offset);

    acquire(&cachepages.lock);
    struct cachepage *res = find_cachepage(dev, ino, alligned_offset);
    release(&cachepages.lock);

    if (!res) return;   // 該当ページなし

    acquiresleep(&res->lock);
    char *page = res->page;
    trace("    - addr=0x%p, page_offset=0x%x, size=0x%x", addr, start_addr, size);
    trace("update_page: memmove from %p to %p with 0x%x bytes", addr, page + start_addr, size);
    memmove(page + start_addr, addr, size);
    releasesleep(&res->lock);
}
