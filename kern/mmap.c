#include <types.h>
#include <arm.h>
#include <mmap.h>
#include <file.h>
#include <fs.h>
#include <memlayout.h>
#include <slab.h>
#include <console.h>
#include <proc.h>
#include <vm.h>
#include <mm.h>
#include <cachepage.h>
#include <string.h>
#include <spinlock.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/mman.h>

/* utils */
#define NOT_PAGEALIGN(a)  ((uint64_t)(a) & (PGSIZE-1))

/*
 * struct vmaリンクリストからnodeを削除して、node->nextをprevに
 * つなぐ。munmap()が呼ばれた時に呼び出される
 */
static void delete_mmap_node(struct proc *p, struct vma *node)
{
    if (p->vmas == NULL) return;

    //trace("delete_mmap_node[%d]: addr=%p", p->pid, node->addr);

    //print_mmap_list(p, "delete node before");

    struct vma *vma, *prev;
    // nodeがp->vmasの先頭vma
    if (node->addr == p->vmas->addr) {
        if (p->vmas->next != NULL)
            p->vmas = p->vmas->next;
        else
            p->vmas = NULL;
    } else {
        vma = prev = p->vmas;
        while (vma) {
            if (node->addr == vma->addr) {
                if (vma->next != NULL)
                    prev->next = vma->next;
                else
                    prev->next = NULL;
                break;
            }
            prev = vma;
            vma = vma->next;
        }
    }
    trace("uvmunmp: pid=%d, addr=%p", p->pid, node->addr);
    uvm_unmap(p->pgdir, (uint64_t)node->addr, (((uint64_t)node->length + PGSIZE - 1) / PGSIZE), 1);
    slab_cache_free(VMA, node);
    node = NULL;

    //print_mmap_list(p, "delete node after");
}

/*
 * struct vmaリンクリスト全体をクリアする。
 * ユーザ空間にあるメモリページをすべて開放しなければ
 * ならない時にexecから呼び出される
 */
void free_mmap_list(struct proc *p)
{
    struct vma* vma = p->vmas;
    struct vma* temp;

    while (vma) {
        temp = vma;
        if (vma->f) {
            fileclose(vma->f);
        }
        delete_mmap_node(p, vma);
        vma = temp->next;
    }
}

// srcからdestにvmaをコピー
static void copy_vma(struct vma *dest, struct vma *src)
{
    dest->addr          = src->addr;
    dest->length        = src->length;
    dest->flags         = src->flags;
    dest->prot          = src->prot;
    dest->offset        = src->offset;
    dest->next          = NULL;

    if (!(src->flags & MAP_ANONYMOUS) && src->f) {
        dest->f = filedup(src->f);
    } else {
        dest->f = NULL;
    }
}

// 指定されたprotectionからpermを作成する
uint64_t get_perm(int prot, int flags)
{
    uint64_t perm;
    if (flags & MAP_ANONYMOUS)
        perm = PTE_USER | PTE_PXN | PTE_PAGE
             | (MT_NORMAL_NC << 2) | PTE_AF | PTE_SH;
    else
        perm = PTE_USER | PTE_PXN | PTE_PAGE
             | (MT_NORMAL << 2) | PTE_AF | PTE_SH;

    if (prot & PROT_READ)
        perm |= PTE_RO;
    if (prot & PROT_WRITE)
        perm &= ~PTE_RO;
    if (!(prot & PROT_EXEC))
        perm |= PTE_UXN;
    if (prot & PROT_NONE || !(prot & (PROT_READ | PROT_WRITE)))
        perm &= ~PTE_USER;

    return perm;
}

/*
 * addr, lengthを持つvmaを作成できるか
 * できる: NULL, できない: MAP_FAILED, 分割: vma
 */
static struct vma *is_usable(void *addr, size_t length)
{
    struct proc *p = thisproc();
    //if (p->pid == 8)
    //    trace("check addr: %p, length: 0x%llx", addr, length);

    // ELF_ET_DYN_BASE未満のアドレスは使用不可
    if (addr < (void *)ELF_ET_DYN_BASE || addr > (void *)USER_TOP) {
        error("addr: %p is invalid", addr);
        return MAP_FAILED;
    }

    struct vma *cursor = p->vmas;
    // vmaが一つも作成されていなければ使用可能
    if (cursor == NULL)
        return NULL;

    while (cursor) {
        //if (p->pid == 8)
        //    trace("vma addr: %p, length: 0x%llx", cursor->addr, cursor->length);

        // 1. 使用済み
        if (addr == cursor->addr) {
            error("addr: %p is used", addr);
            return MAP_FAILED;
        }

        // 2. addr + length == cursor->addr + cursor->length: 分割
        if (addr + length == cursor->addr + cursor->length) {
            return cursor;
        }

        // 3: 右端が最左のvmaより小さい
        if (addr + length <= cursor->addr)
            return NULL;
        // 3: 左端が最左のvmaより大きい、かつ、次のvmaがないか、右端が次のvmaより小さい
        if (cursor->addr + cursor->length <= addr && (cursor->next == 0 || addr + length <= cursor->next->addr))
            return NULL;
        cursor = cursor->next;
    }

    error("mmap is full: addr: %p", addr);
    return MAP_FAILED;
}

/*
 * offsetからファイルの内容をaddrにlengthマッピングする
 */
static long map_file_pages(struct proc *p, void *addr, uint64_t length, uint64_t perm, struct file *f, off_t offset)
{
    long ret = 0;
    size_t mapsize, size = length;
    struct cachepage *cpage;
    uint64_t cur;
    trace("called: addr: 0x%llx, length: 0x%x, perm: 0x%x, f_ino: %d, f_size: 0x%x, offset: 0x%x", addr, length, perm, f->ip->inum, f->vnode->size, offset);
    // サイズが0のファイルに書き込むために1ページ分uvm_map()する
    if (f->ip->size == 0) {
        char *mem = kalloc(1);
        if (!mem) {
            error("no memory");
            return -ENOMEM;
        }
        memset(mem, 0, PGSIZE);
        if ((ret = uvm_map(p->pgdir, addr, PGSIZE, V2P(mem))) < 0) {
            error("0 size uvm_map failed: addr: %p, mem: %p", addr, V2P(mem));
            kfree(mem);
            return ret;
        }
        return 0;
    }

    for (cur = 0; cur < length; cur += PGSIZE) {
        mapsize = PGSIZE > size ? size : PGSIZE;
        trace("cur: %lld, f: %d, offset+cur: 0x%llx", cur, f->ip->inum, offset+cur);
        if ((cpage = get_cachepage(f, offset + cur)) == NULL) {
            error("get_cachepage failed");
            ret = -1;
            goto err;
        }
        trace("cpage->page: 0x%llx, V2P(cpage->page): 0x%llx, ino: %lld, dev: 0x%x, offset: 0x%x, mapsize: 0x%x", cpage->page, V2P(cpage->page), cpage->ino, cpage->dev, cpage->offset, mapsize);
        trace("cpage->page[49-50]: 0x%02x%02x", cpage->page[49], cpage->page[50]);
        if ((ret = uvm_map(p->pgdir, addr + cur, mapsize, V2P(cpage->page))) < 0) {
            error("uvm_map failed: addr: %p, len: 0x%x, mem: %p", addr + cur, mapsize, V2P(cpage->page));
            ret = -1;
            goto err;
        }
        releasesleep(&cpage->lock);
        size -= mapsize;
    }
    trace("ok");
    return 0;

#if 0
    for (cur = 0; cur < length; cur += PGSIZE) {
        f->offset = offset;
        int len = (f->vnode->size - f->offset) > PGSIZE ? PGSIZE : (f->vnode->size - f->offset);
        if (len == 0) break;
        trace("size: 0x%x, offset: 0x%x, len: 0x%x, cur: 0x%x", f->vnode->size, f->offset, len, cur);
        //trace("addr=%p, length=0x%x, offset=0x%x", addr, length, offset);

        char *mem = kalloc(1);
        if (!mem) {
            error("no memory");
            ret = -ENOMEM;
            goto err;
        }
        memset(mem, 0, PGSIZE);

        ret = fileread(f, mem, len, 0);
        if (ret < 0 || ret != len) {
            error("fileread failed: len: 0x%x, ret: 0x%x", len, ret);
            kfree(mem);
            goto err;
        }
        trace("pid[%d] mapping: addr=%p, mem=%p (V2P: %p), offset: 0x%x, len: 0x%x", p->pid, addr+cur, mem, V2P(mem), offset, len);

        if ((ret = copy_cachepage(f, offset, )))
        // メモリをユーザプロセスにマッピング
        if ((ret = uvm_map(p->pgdir, addr + cur, len, V2P(mem))) < 0) {
            error("uvm_map failed: addr: %p, len: 0x%x, mem: %p", addr + cur, len, V2P(mem));
            kfree(mem);
            goto err;
        }
        offset += len;
    }
    return 0;
#endif

err:
    if (cur != 0) {
        uvm_unmap(p->pgdir, (uint64_t)addr, cur / PGSIZE, 1);
    }
    return ret;

}

// 無名ページに（複数）ページを割り当てる
static long map_anon_pages(struct proc *p, void *addr, uint64_t length, uint64_t perm)
{
    long ret;
    uint64_t cur;

    for (cur = 0; cur < length; cur += PGSIZE) {
        char *page = kalloc(1);
        if (!page) {
            error("map_anon_page: memory exhausted");
            ret = -ENOMEM;
            goto err;
        }
        memset(page, 0, PGSIZE);
        trace("pid[%d] map addr %p to page %p with perm 0x%llx", p->pid, addr+cur, page, perm);
        if (uvm_map(p->pgdir, addr + cur, PGSIZE, V2P(page)) < 0) {
            kfree(page);
            ret = -EINVAL;
            goto err;
        }
    }

    return 0;

err:
    if (cur != 0) {
        uvm_unmap(p->pgdir, (uint64_t)addr, cur / PGSIZE, 1);
    }
    return ret;
}

/* addrにメモリを割り当てる */
long mmap_load_pages(void *addr, uint64_t length, int prot, int flags, struct file *f, off_t offset)
{
    struct proc *p = thisproc();
    trace("addr: %p, length: 0x%llx, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%llx", addr, length, prot, flags, f ? f->ip->inum : -1, offset);
    uint64_t perm = get_perm(prot, flags);
    //if (flags & MAP_SHARED) perm |= PTE_W;

    if (flags & MAP_ANONYMOUS)
        return map_anon_pages(p, addr, length, perm);
    else
        return map_file_pages(p, addr, length, perm, f, offset);

}


// vmaのサイズをsizeにする（拡大する場合は、拡大可能であることが保証されていること）
// sizeはPGSIZEの整数倍とする
static long scale_vma(struct vma *vma, uint64_t size)
{
    long error;

    assert((size % PGSIZE)==0);

    if (vma->length == size) return 0;

    if (vma->length < size) { // 拡大
        if ((error = mmap_load_pages(vma->addr + vma->length, size - vma->length, vma->prot, vma->flags, vma->f, vma->offset)) < 0)
            return error;
    } else {                        // 縮小
        uint64_t dstart = PGROUNDDOWN((uint64_t)vma->addr + vma->length);
        uint64_t dpages = (vma->length - dstart + PGSIZE - 1) / PGSIZE;
        //int free = (vma->flags & MAP_SHARED) ? 0 : 1;
        if ((uint64_t)vma->addr + size <= dstart && dpages > 0) {
            uvm_unmap(thisproc()->pgdir, dstart, dpages, 1);
        }
    }
    vma->length = size;

    return 0;
}

// struct vma listを出力
void print_mmap_list(struct proc *p, const char *title)
{
    int i = 0;
    debug("[INFO] pid[%d]: vma list (%s) at %p\n", p->pid, title, p->vmas);

    struct vma *vma = p->vmas;
    while (vma) {
        debug(" - vma[%d]: addr=%p, length=0x%x, prot=0x%x, flags=0x%x, f=%d, offset=0x%x\n",
            ++i, vma->addr, vma->length, vma->prot, vma->flags, (vma->f ? vma->f->ip->inum : 0), vma->offset);
        vma = vma->next;
    }
}

// 親プロセスから子プロセスにvmaをコピー.
long copy_vmas(struct proc *parent, struct proc *child)
{
    struct vma *node = parent->vmas;
    struct vma *cnode = NULL, *tail = 0;

    while (node) {
        struct vma *vma = slab_cache_alloc(VMA);
        if (vma == (struct vma *)0)
            return -ENOMEM;

        // vmaのコピー
        copy_vma(vma, node);

        if (cnode == 0)
            cnode = vma;
        else
            tail->next = vma;

        tail = vma;
        node = node->next;
    }

    child->vmas = cnode;

    return 0;
}

/* startを含むvmaを返す */
struct vma *find_vma(struct proc *p, void *start)
{
    struct vma *vma = p->vmas;

    while (vma) {
        if (vma->addr <= start && start < (vma->addr + vma->length))
            return vma;
        vma = vma->next;
    }
    return NULL;
}

/* startを含むvmaを返し、そのprevをセットする */
#if 0
static struct vma *find_vma_prev(struct proc *p, void *start, struct vma **pprev)
{
    struct vma *vma = p->vmas;
    struct vma *prev = NULL;

    while (vma) {
        if (vma->addr <= start && start < (vma->addr + vma->length)) {
            *pprev = prev;
            return vma;
        }
        prev = vma;
        vma = vma->next;
    }
    return NULL;
}
#endif

/* addr + length はp->vmasに含まれるか */
boolean is_vma(struct proc *p, void *addr, uint64_t length)
{
    struct vma *vma = find_vma(p, addr);
    if (vma == NULL)
        return false;
    return ((uint64_t)addr + length <= (uint64_t)vma->addr + vma->length);
}

#if 0
// 遅延mapを実装（trap.cから呼び出される）
// 該当するアドレスを含む1ページ分の割り当て/ファイル読み込みをする
long alloc_mmap_page(struct proc *p, uint64_t addr,  uint64_t scause) {
    off_t offset = 0;
    struct caced_page *cpage;

    uint64_t rounddown = PGROUNDDOWN(addr);
    struct vma *vma = find_vma(p, (void *)rounddown);
    if (vma == NULL) {
        error("no vma with addr 0x%llx", addr);
        return -1;
    }

    if (scause == SCAUSE_PAGE_STORE && (vma->prot & PROT_WRITE) == 0) {
        error("vma is not writable");
        return -1;
    }

    // ファイルの場合はpagecacheから読み込んでマッピング
    if (vma->f) {
        // ファイルオフセットの計算
        offset = vma->offset + ((rounddown - (uint64_t)vma->addr) / PGSIZE) * PGSIZE;
        cpage = get_page(vma->f->ip, offset);
        if (IS_ERRO(cpage)) {
            error("get_page: inum: %d, offset: 0x%x, errno: %ld", vma->f->ip->inum, offset, (long)cpage);
            return -1;
        }
        return uvm_map(p->pgdir, offset, PGSIZE, (uint64_t)cpage->page, get_perm(vma->prot, vma->flags));
    }
    // ファイルでない場合はページを割り当ててマッピング
    return map_anon_pages(p, rounddown, PGSIZE, get_perm(vma->prot, vma->flags));

    // ファイルオフセットの計算
    if (vma->f) {
        offset = vma->offset + ((rounddown - (uint64_t)vma->addr) / PGSIZE) * PGSIZE;
    }
    trace("pid[%d] addr: 0x%llx, rounddown: 0x%llx, offset: %ld", p->pid, addr, rounddown, offset);
    if (mmap_load_pages((void *)rounddown, PGSIZE, vma->prot, vma->flags, vma->f, offset) < 0) {
        error("loading page failed: addr: 0x%llx, length: 0x%x, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%x",
            rounddown, PGSIZE, vma->prot, vma->flags, vma->f ? vma->f->ip->inum : -1, offset);
        return -1;
    }
    return 0;
}
#endif

// Copy on Write
long copy_vma_pages(void *addr, size_t length, uint64_t perm)
{
    uint64_t *pte;
    trace("copy_mmap_pages: addr=%p, length=0x%llx, perm=0x%llx\n", addr, length, perm);
    void *start  = addr;
    for (; start < addr + length; start += PGSIZE) {
        pte = walk(thisproc()->pgdir, start, 0);
        if (pte == 0) { warn("copy_mmap_pages: pte = 0\n"); return -EINVAL; }
        uint64_t pa = PTE_ADDR(*pte);
        char *page = kalloc(1);
        if (!page) { warn("copy_mmap_pages: no page available\n"); return -ENOMEM; }
        memmove(page, P2V(pa), PGSIZE);
        *pte = V2P(page) | perm;
        trace("- start=%p, pte=%p, *pte=0x%llx\n", start, pte, *pte);
    }
    return 0;
}

#if 0
int alloc_cow_page(uint64_t *pgdir, uint64_t va)
{
    if (va >= MAXVA) {
        error("va: 0x%llx over MAXVA", va);
        return -1;
    }

    pte_t *pte = walk(pagetable, va, 0);
    // COW領域でない
    if (pte == 0 || (*pte & PTE_V) == 0
    || (*pte & PTE_U) == 0 || (*pte & PTE_COW) == 0) {
        return 1;
    }

    uint64_t flags = PTE_FLAGS(*pte);
    uint64_t pa = PTE2PA(*pte);
    uint64_t va0 = PGROUNDDOWN(va);
    char *mem;

    // 複数のプロセスがこのページを参照しているのでコピーが必要
    if (page_refcnt_get((void *) pa) > 1) {
        if ((mem = kalloc(1)) == 0) {
            error("no memory");
            return -1;
        }
        memmove(mem, (char*)pa, PGSIZE);
        uvm_unmap(pagetable, va0, 1, 1);
        flags &= ~PTE_COW;
        flags |= PTE_W;
        if (uvm_map(pagetable, va0, PGSIZE, (uint64_t)mem, flags) != 0) {
            kfree(mem);
            error("uvm_map error: va=0x%llx", va);
            return -1;
        }
        trace("alloc ok: va=0x%llx, pa: %p", va, mem);
        fence_i();
        return 0;
    } else if (page_refcnt_get((void *) pa) == 1){
        *pte |= PTE_W;
        *pte &= ~PTE_C;
        trace("flag updated: pa: 0x%llx", pa);
        fence_i();
        return 0;
    } else {
        error("unknown error: va=0x%llx", va);
        return -1;
    }
}
#endif

// musl libc.so対応
//  addr + length がvmaの最高位アドレスに等しい場合に、2つに分割する。
static long devide_vma(struct vma *vma, void *addr, size_t length, int prot, int flags, struct file *f, off_t offset)
{
    struct vma *backup;
    long error;

    if (NOT_PAGEALIGN(addr) || NOT_PAGEALIGN(length))
        return (long)MAP_FAILED;

    if ((addr + length) != (vma->addr + vma->length))
        return (long)MAP_FAILED;

    backup = slab_cache_alloc(VMA);
    if (backup == NULL)
        return (long)MAP_FAILED;
    memmove((void *)backup, vma, sizeof(struct vma));

    if ((error = munmap(vma->addr, vma->length)) < 0) {
        slab_cache_free(VMA, backup);
        return error;
    }

    mmap(backup->addr, backup->length - length, backup->prot, (backup->flags | MAP_FIXED), backup->f, backup->offset);
    slab_cache_free(VMA, backup);
    return (long)mmap(addr, length, prot, (flags | MAP_FIXED), f,  offset);
}

static int change_prot(uint64_t start, uint64_t end, int prot)
{
    uint64_t va;
    uint64_t *pte;

    for (va = start; va < end; va+= PGSIZE) {
        pte = walk(thisproc()->pgdir, (void *)va, 0);
        if ((*pte & PTE_RO) && (prot == PROT_WRITE)) {
            error("wrong prot: prot: 0x%x, *pte: 0x%x", prot, *pte & 0xff);
            return -EACCES;
        }

        *pte &= ~(PTE_RO | PTE_USER);
        if (prot != PROT_NONE) {
            if (prot & PROT_READ && !(prot & PROT_WRITE))
                *pte |= PTE_RO;
            else
                *pte |= PTE_RW;
            *pte |= PTE_USER;
        }
    }

    return 0;
}

// sys_mmapのメイン関数
void *mmap(void *addr, size_t length, int prot, int flags, struct file *f, off_t offset)
{
    struct proc *p = thisproc();
    struct vma *node, *dev_vma;
    long error = -EINVAL;

    trace("addr: %p, length: 0x%llx, prot: 0x%x, flags: 0x%x, f: %d, off: 0x%llx",
        addr, length, prot, flags, f ? f->ip->inum : 0, offset);

    // MAP_FIXEDの指定アドレスはページ境界にあり、割り当て領域がMMAPエリア内に入ること
    // 1. addrを確定する
    // 1.1. アドレスが指定されている場合
    if (addr != 0) {
        // 1.1.1 指定アドレス+指定サイズはUSER_TOPを超えないこと
        // upper_addr: addr + sizeがMMAPTOPを超えないかチェックするための変数
        uint64_t upper_addr = PGROUNDUP(PGROUNDUP((uint64_t)addr) + length);
        if (upper_addr > USER_TOP) {
            error("addr: %p + length: 0x%llx overs USER_TOP", addr, length);
            return (void *)-EINVAL;
        }
        // 1.1.2 MAP_FIXEDが指定されている場合
        if (flags & MAP_FIXED) {
            // 1.1.2.1 アドレスはページ境界にあること
            if (NOT_PAGEALIGN(addr)) {
                error("fixed address should be page align: %p", addr);
                return (void *)-EINVAL;
            }
            // ユーザアクセス禁止のpteを作成: mappingなし
            if (prot == PROT_NONE) {
                for (int i = 0; i < PGROUNDUP(length) / PGSIZE; i++) {
                    uint64_t *pte = walk(p->pgdir, addr + i * PGSIZE, 1);
                    if (pte == NULL) {
                        error("PROT_NONE invalid addr");
                        return (void *)-EINVAL;
                    }
                    // ユーザアクセスを禁止にする
                    *pte &= ~PTE_USER;
                }
                return addr;
            } else {
                // 1.1.2.2 指定されたアドレスが使用可能なこと
                dev_vma = is_usable(addr, length);
                if (dev_vma == MAP_FAILED) {
                    error("addr %p is not available", addr);
                    return (void *)-EINVAL;
                } else if (dev_vma == NULL) {
                    /* ok */
                } else {
                    return (void *)devide_vma(dev_vma, addr, length, prot, flags, f, offset);
                }
            }
        // 1.1.3 MAP_FIXEDが指定されていない場合は、アドレスを丸め下げ
        } else {
            addr = (void *)PGROUNDDOWN((uint64_t)addr);
            goto select_addr;
        }
    // 1.2. アドレスが指定されていない場合
    } else {
        // 1.2.1 最初のアドレス候補
        if (p->vmas && (uint64_t)p->vmas->addr >= MMAP_BASE)
            addr = p->vmas->addr;
        else
            addr = (void *)MMAP_BASE;
select_addr:
        node = p->vmas;
        while (node) {
            trace("- addr=0x%x, node->addr=0x%x, node->next->addr=0x%x", addr, node->addr, node->next ? node->next->addr : NULL);
            // 1.2.1 作成マッピングが現在のノードアドレスより小さい場合はこの候補を使用する
            if (addr + PGROUNDUP(length) <= node->addr)
                break;
            // 1.2.2 次のマッピングがない、または次のマッピングとの間に置ける場合はこの候補を使用する
            if (node->addr + node->length <= addr && (node->next == 0 || addr + PGROUNDUP(length) <= node->next->addr))
                break;
            // 1.2.3 それ以外は、現在のマッピングの右端をアドレス候補とする
            if (addr <= node->addr + node->length)
                addr = node->addr + node->length;
            // 1.2.4 次のマッピングと比較する
            node = node->next;
        }
    }
    // 1.3 決定したアドレスがマップ範囲に含まれていることをチェックする
    if (addr + PGROUNDUP(length) > (void *)USER_TOP)
        return (void *)-ENOMEM;
    trace("select addr: %p", addr);
    // 2. 新規vmaを作成する
    // 2.1 vmaのためのメモリを割り当てる
    struct vma *vma = slab_cache_alloc(VMA);
    if (vma == NULL)
        return (void *)-ENOMEM;
    // 2.2 vmaにデータを設定する
    vma->addr   = addr;
    vma->length = length;
    vma->flags  = flags;
    vma->offset = offset;
    vma->prot   = prot;
    vma->next   = NULL;

    // 2.2 fを設定
    if (f) {
        filedup(f);
        if (!(flags & MAP_ANONYMOUS)) {
            vma->f = filedup(f);  // munmapとofile-closeで2回decrementされる
        } else {
            goto out;
        }
    } else {
        vma->f = NULL;
    }

    // 3. p->vmasに作成したvmaを追加する
    // 3.1 これがプロセスの最初のvmaの場合はp->vmasに追加する
    if (p->vmas == NULL) {
        trace("first vma: addr: %p", vma->addr);
        p->vmas = vma;
        goto load_pages;
    }

    // 3.2 そうでない場合は適切な位置に追加して、p->vmasを更新する
    node = p->vmas;
    struct vma *prev = p->vmas;
    while (node) {
        trace("update vmas: addr=%p, node->addr=%p\n", addr, node->addr);
        if (addr < node->addr) {
            vma->next = node;
            prev = vma;
            break;
        } else if (addr > node->addr && node->next && addr < node->next->addr) {
            vma->next = node->next;
            node->next = vma;
            break;
        }
        if (!node->next) {
            node->next = vma;
            break;
        }
        node = node->next;
    }
    p->vmas = prev;

load_pages:

    if ((error = mmap_load_pages(addr, length, prot, flags, f, offset)) < 0) {
        trace("mmap_load_pages: error: %d, addr: %p, length: 0x%llx, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%llx", error, addr, length, prot, flags, f ? f->ip->inum : -1, offset);
        goto out;
    }

    vma->addr = addr;
    // ファイルオフセットを正しく処理するためにlengthはここで切り上げる
    vma->length = PGROUNDUP(length);

    trace("return addr: %p, length: 0x%llx, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%x",
        vma->addr, vma->length, vma->prot, vma->flags, vma->f ? vma->f->ip->inum : 0, vma->offset);
    //print_mmap_list(p, "mmap");
    if ((uint64_t)vma->addr == 0xffdfffff0000) {
        trace("addr[49-50]: 0x%02x%02x", ((char *)vma->addr)[49],((char *)vma->addr)[50]);
    }

    return vma->addr;

out:
    if (f) fileclose(f);
    slab_cache_free(VMA, vma);
    return (void *)error;
}

// sys_munmapのメイン関数
long munmap(void *addr, size_t length)
{
    struct proc *p = thisproc();

    trace("called addr: %p, length: 0x%llx", addr, length);

    // addrはページ境界になければならない
    if (NOT_PAGEALIGN(addr))
        return -EINVAL;

    // lengthは境界になくてもよいが、処理は境界に合わせる
    length = PGROUNDUP(length);

    trace("pid[%d] addr=%p, length=0x%x", p->pid, addr, length);

    struct vma *vma = find_vma(p, addr);
    if (vma == NULL) {
        warn("no vma with addr %p", addr);
        return 0;
    }

    if (vma->length < length) {
        warn("length 0x%llx is bigger than vma->length 0x%llx", length, vma->length);
        length = vma->length;
    }

    trace(" - found: addr=%p, length=0x%x", vma->addr, vma->length);
    trace(" - delete 0x%llx bytes", length);

    // MAP_SHARED領域で背後にあるファイルに書き込みがあったら書き戻す
    //int len = (f->vnode->size - f->offset) > PGSIZE ? PGSIZE : (f->vnode->size - f->offset);
    if (vma->flags & MAP_SHARED && vma->f && vma->prot & PROT_WRITE) {
        for (uint64_t ra = (uint64_t)addr; ra < (uint64_t)addr + length; ra += PGSIZE) {
            //uint64_t *pte = walk(p->pgdir, (void *)ra, 0);
            //if (!pte) panic("no pte");
            //uint64_t flags = PTE_FLAGS(*pte);
            off_t offset = vma->offset + ra - (uint64_t)vma->addr;
            trace("writeback: f: %d, offset: 0x%x, ra: 0x%llx", vma->f->ip->inum, offset, ra);
            if (filewriteback(vma->f, offset, ra) < 0) {
                error("failed writeback");
                return -EACCES;
            }
            if (vma->f->ip->size <= offset + PGSIZE)
                break;
        }
    }

    // vmaを全部削除
    if (vma->length == length) {
        trace("delete WHOLE");
        if (vma->f) {
            fileclose(vma->f);
        }
        delete_mmap_node(p, vma);
    // vmaの一部を削除
    } else {
        trace("delete PART");
        uvm_unmap(p->pgdir, (uint64_t)addr, length / PGSIZE, 1);
        if (vma->addr == addr) {
            vma->addr += length;
            vma->offset += length;
        }
        vma->length -= length;
        trace("new vma: addr=%p, length=0x%x", vma->addr, vma->length);
    }
    //print_mmap_list(p, "munmap");
    //vm_stat(p->pgdir);

    return 0;
}

void *mremap(void *old_addr, size_t old_length, size_t new_length, int flags, void *new_addr)
{
    void *mapped_addr;
    long error = -EINVAL;
    //trace("- remap: old_addr=%p, old_length=0x%x, new_length=0x%x, flags=0x%x",
    //    old_addr, old_length, new_addr, flags);
    struct vma *vma = find_vma(thisproc(), old_addr);
    if (vma == NULL) return (void *)error;

    if (vma->length != old_length) return  (void *)error;

    new_length = PGROUNDUP(new_length);

    // 1: その場で拡大（縮小）可能の場合
    if (!vma->next || vma->addr + new_length <= vma->next->addr) {
        if (flags & MREMAP_FIXED) {
            if ((error = munmap(old_addr, old_length)) < 0)
                return  (void *)error;
            return (void *)mmap(new_addr, new_length, vma->prot, (vma->flags | MAP_FIXED), vma->f,  vma->offset);
        } else {
            if ((error = scale_vma(vma, new_length)) < 0)
                return (void *)error;
            //uvm_switch(thisproc()->pagetable);
            return vma->addr;
        }
    }
    // 2: その場では拡大できない場合
    if (!(flags & MREMAP_MAYMOVE)) return (void *)error;

    if (flags & MREMAP_FIXED)
        mapped_addr = (void *)mmap(new_addr, new_length, vma->prot, (vma->flags | MAP_FIXED), vma->f, vma->offset);
    else
        mapped_addr = (void *)mmap(new_addr, new_length, vma->prot, (vma->flags & ~MAP_FIXED), vma->f, vma->offset);

    if (IS_ERR(mapped_addr)) {
        return mapped_addr;

    }

    memmove(new_addr, vma->addr, vma->length);
    if ((error = msync(mapped_addr, new_length, MS_SYNC)) < 0)
        return (void *)error;

    if ((error = munmap(vma->addr, vma->length)) < 0)
        return (void *)error;
    //uvm_switch(thisproc()->pagetable);
    //print_mmap_list(thisproc(), "mremap");
    return mapped_addr;
}

long mprotect(void *addr, size_t length, int prot)
{
    struct proc *p = thisproc();
    struct vma *vma, *new1, *new2;
    int newprot;

    trace("addr: %p, length: 0x%llx, prot: 0x%x", addr, length, prot);

    uint64_t addrp = (uint64_t)addr;
    if (addrp <= p->sz) {
        return change_prot(addrp, addrp + length, prot);
    }

    if (!is_vma(p, addr, length)) {
        error("invalid vma: addr: %p, length: 0x%llx", addr, length);
        return -EFAULT;
    }

    vma = find_vma(p, addr);

    if ((vma->prot & (PROT_READ | PROT_WRITE)) == PROT_READ && (prot & PROT_WRITE)) {
        error("wrong prot: prot: 0x%x, regon->prot: 0x%x", prot, vma->prot);
        return -EACCES;
    }

    length = PGROUNDUP(length);

    if (vma->addr == addr && vma->length == length && vma->prot == prot)
        return 0;

    newprot = vma->prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC);
    newprot |= prot;

    // 1: 先頭も末尾も同じ
    if (vma->addr == addr && vma->length == length) {
        vma->prot = newprot;
    // 2: 先頭が同じ
    } else if (vma->addr == addr) {
        new2 = slab_cache_alloc(VMA);
        if (!new2) return -ENOMEM;

        new2->addr = vma->addr + length;
        new2->length = vma->length - length;
        new2->flags = vma->flags;
        new2->prot = vma->prot;
        new2->f = vma->f;
        new2->offset = vma->f ? vma->offset + length: 0;
        new2->next = vma->next;

        vma->addr = vma->addr;
        vma->length = length;
        vma->prot = newprot;
        vma->next = new2;
    // 3: 末尾が同じ
    } else if ((addr + length) == (vma->addr + vma->length)) {
        new1 = slab_cache_alloc(VMA);
        if (!new1) return -ENOMEM;

        new1->addr = vma->addr;
        new1->length = vma->length - length;
        new1->flags = vma->flags;
        new1->prot = vma->prot;
        new1->f = vma->f;
        new1->offset = vma->f ? vma->offset : 0;
        new1->next = vma;

        vma->addr = addr;
        vma->length = length;
        vma->prot = newprot;
    // 4: 領域の真ん中
    } else {
        new1 = slab_cache_alloc(VMA);
        if (!new1) return -ENOMEM;
        new2 = slab_cache_alloc(VMA);
        if (!new2) return -ENOMEM;

        new1->addr = vma->addr;
        new1->length = (uint64_t)addr - (uint64_t)vma->addr;
        new1->flags = vma->flags;
        new1->prot = vma->prot;
        new1->f = vma->f;
        new1->offset = vma->f ? vma->offset : 0;
        new1->next = vma;

        new2->addr = addr + length;
        new2->length = vma->length - new1->length - length;
        new2->flags = vma->flags;
        new2->prot = vma->prot;
        new2->f = vma->f;
        new2->offset = vma->f ? vma->offset + new1->length + length: 0;
        new2->next = vma->next;

        vma->addr = addr;
        vma->length = length;
        vma->prot = newprot;
        vma->next = new2;
    }

    return change_prot((uint64_t)vma->addr, (uint64_t)vma->addr + vma->length, prot);

    //    print_mmap_list(p, "mprotect");
}

long msync(void *addr, size_t length, int flags)
{
    struct proc *p = thisproc();
    long error = -EINVAL;

    if (!(flags & MS_ASYNC) && !(flags & MS_SYNC)) {
        flags |= MS_ASYNC;
    }

    if (flags & MS_ASYNC) {
        // Since  Linux  2.6.19, MS_ASYNC  is  in  fact  a no-op (from man(2))
        return 0;
    }

    // addrはページ境界になければならない。
    if (NOT_PAGEALIGN((uint64_t)addr)) return error;

    struct vma *vma = find_vma(p, addr);
    if (vma == NULL) {
        warn("no vma with addr %p", addr);
        return 0;
    }

    if (vma->length < length) {
        warn("length 0x%llx is bigger than vma->length 0x%llx", length, vma->length);
        length = vma->length;
    }

    // ファイルが背後にあり共有されているマッピングで変更があった場合は書き戻す
    if (vma->flags & MAP_SHARED && vma->f && vma->prot & PROT_WRITE) {
        for (uint64_t ra = (uint64_t)addr; ra < (uint64_t)addr + length; ra += PGSIZE) {
            uint64_t *pte = walk(p->pgdir, (void *)ra, 0);
            if (!pte) panic("mysync: no pte");
            off_t offset = vma->offset + ra - (uint64_t)vma->addr;
            if (filewriteback(vma->f, offset, ra) < 0) {
                error("failed writeback");
                return -EACCES;
            }

            if (vma->f->ip->size <= offset + PGSIZE)
                break;
        }
    }

    return 0;
}
