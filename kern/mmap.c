#include <types.h>
#include <arm.h>
#include <mmap.h>
#include <file.h>
#include <fs.h>
#include <memlayout.h>
#include <slab.h>
#include <console.h>
#include <sched.h>
#include <vm.h>
#include <mm.h>
#include <cachepage.h>
#include <string.h>
#include <spinlock.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/mman.h>

/* utils */
#define NOT_PAGEALIGN(a)  ((a) & (PGSIZE-1))

/*
 * struct vmaリンクリストからnodeを削除して、node->nextをprevに
 * つなぐ。munmap()が呼ばれた時に呼び出される
 */
static void delete_mmap_node(struct task_struct *p, struct vma *node)
{
    if (p->mm->vmas == NULL) return;

    //trace("delete_mmap_node[%d]: addr=0x%lx", p->pid, node->addr);

    //print_mmap_list(p, "delete node before");

    struct vma *vma, *prev;
    // nodeがp->mm->vmasの先頭vma
    if (node->addr == p->mm->vmas->addr) {
        if (p->mm->vmas->next != NULL)
            p->mm->vmas = p->mm->vmas->next;
        else
            p->mm->vmas = NULL;
    } else {
        vma = prev = p->mm->vmas;
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
    trace("uvmunmp: pid=%d, addr=0x%lx", p->pid, node->addr);
    uvm_unmap(p->mm, node->addr, ((node->length + PGSIZE - 1) / PGSIZE), 1);
    slab_cache_free(VMA, node);
    node = NULL;

    //print_mmap_list(p, "delete node after");
}

/*
 * struct vmaリンクリスト全体をクリアする。
 * ユーザ空間にあるメモリページをすべて開放しなければ
 * ならない時にexitから呼び出される
 */
void free_mmap_list(struct task_struct *p)
{
    struct vma* vma;
    struct vma* temp;

    if (!p->mm || !p->mm->vmas) {
        trace("%s does not have mm or mm->vmas", p->name);
        return;
    }

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
static struct vma *is_usable(uintptr_t addr, size_t length)
{
    struct task_struct *p = myproc();

    // ELF_ET_DYN_BASE未満のアドレスは使用不可
    if (addr < ELF_ET_DYN_BASE || addr > USER_TOP) {
        error("addr: 0x%lx is invalid", addr);
        return MAP_FAILED;
    }

    struct vma *cursor = p->mm->vmas;
    // vmaが一つも作成されていなければ使用可能
    if (cursor == NULL) {
        return NULL;
    }

    while (cursor) {
        //if (p->pid == 8)
        //    trace("vma addr: 0x%lx, length: 0x%lx", cursor->addr, cursor->length);

        // 1. 使用済み
        if (addr == cursor->addr) {
            error("addr: 0x%lx is used", addr);
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

    error("mmap is full: addr: 0x%lx", addr);
    return MAP_FAILED;
}

/*
 * offsetからファイルの内容をaddrにlengthマッピングする
 */
static int map_file_pages(struct task_struct *p, uintptr_t addr, uint64_t length, uint64_t perm, struct file *f, off_t offset)
{
    int ret = 0;
    size_t mapsize, size = length;
    struct cachepage *cpage;
    uint64_t cur;
    trace("called: addr: 0x%lx, length: 0x%x, perm: 0x%x, f_ino: %d, f_size: 0x%x, offset: 0x%x", addr, length, perm, f->ip->inum, f->vnode->size, offset);
    // サイズが0のファイルに書き込むために1ページ分uvm_map()する
    if (f->ip->size == 0) {
        char *mem = kalloc(1);
        if (!mem) {
            error("no memory");
            return -ENOMEM;
        }
        memset(mem, 0, PGSIZE);
        if ((ret = uvm_map(p->mm, addr, PGSIZE, V2P(mem))) < 0) {
            error("0 size uvm_map failed: addr: 0x%lx, mem: 0x%lx", addr, V2P(mem));
            kfree(mem);
            return ret;
        }
        return 0;
    }

    for (cur = 0; cur < length; cur += PGSIZE) {
        mapsize = PGSIZE > size ? size : PGSIZE;
        trace("cur: %lld, f: %d, offset+cur: 0x%lx", cur, f->ip->inum, offset+cur);
        if ((cpage = get_cachepage(f, offset + cur)) == NULL) {
            error("get_cachepage failed");
            ret = -1;
            goto err;
        }
        trace("cpage->page: 0x%lx, V2P(cpage->page): 0x%lx, ino: %lld, dev: 0x%x, offset: 0x%x, mapsize: 0x%x", cpage->page, V2P(cpage->page), cpage->ino, cpage->dev, cpage->offset, mapsize);
        trace("cpage->page[49-50]: 0x%02x%02x", cpage->page[49], cpage->page[50]);
        if ((ret = uvm_map(p->mm, addr + cur, mapsize, V2P(cpage->page))) < 0) {
            error("uvm_map failed: addr: 0x%lx, len: 0x%x, mem: 0x%lx", addr + cur, mapsize, V2P(cpage->page));
            ret = -1;
            goto err;
        }
        releasesleep(&cpage->lock);
        size -= mapsize;
    }
    trace("ok");
    return 0;

err:
    if (cur != 0) {
        uvm_unmap(p->mm, addr, cur / PGSIZE, 1);
    }
    return ret;

}

// 無名ページに（複数）ページを割り当てる
static int map_anon_pages(struct task_struct *p, uintptr_t addr, uint64_t length, uint64_t perm)
{
    int ret;
    uint64_t cur;

    for (cur = 0; cur < length; cur += PGSIZE) {
        char *page = kalloc(1);
        if (!page) {
            error("map_anon_page: memory exhausted");
            ret = -ENOMEM;
            goto err;
        }
        memset(page, 0, PGSIZE);
        trace("pid[%d] map addr 0x%lx to page 0x%lx with perm 0x%lx", p->pid, addr+cur, page, perm);
        if (uvm_map(p->mm, addr + cur, PGSIZE, V2P(page)) < 0) {
            kfree(page);
            ret = -EINVAL;
            goto err;
        }
    }

    return 0;

err:
    if (cur != 0) {
        uvm_unmap(p->mm, addr, cur / PGSIZE, 1);
    }
    return ret;
}

/* addrにメモリを割り当てる */
int mmap_load_pages(uintptr_t addr, uint64_t length, int prot, int flags, struct file *f, off_t offset)
{
    struct task_struct *p = myproc();
    trace("addr: 0x%lx, length: 0x%lx, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%lx", addr, length, prot, flags, f ? f->ip->inum : -1, offset);
    uint64_t perm = get_perm(prot, flags);
    //if (flags & MAP_SHARED) perm |= PTE_W;

    if (flags & MAP_ANONYMOUS)
        return map_anon_pages(p, addr, length, perm);
    else
        return map_file_pages(p, addr, length, perm, f, offset);

}


// vmaのサイズをsizeにする（拡大する場合は、拡大可能であることが保証されていること）
// sizeはPGSIZEの整数倍とする
static int scale_vma(struct vma *vma, uint64_t size)
{
    int error;

    assert((size % PGSIZE)==0);

    if (vma->length == size) return 0;

    if (vma->length < size) { // 拡大
        if ((error = mmap_load_pages(vma->addr + vma->length, size - vma->length, vma->prot, vma->flags, vma->f, vma->offset)) < 0)
            return error;
    } else {                        // 縮小
        uint64_t dstart = PGROUNDDOWN(vma->addr + vma->length);
        uint64_t dpages = (vma->length - dstart + PGSIZE - 1) / PGSIZE;
        //int free = (vma->flags & MAP_SHARED) ? 0 : 1;
        if (vma->addr + size <= dstart && dpages > 0) {
            uvm_unmap(myproc()->mm, dstart, dpages, 1);
        }
    }
    vma->length = size;

    return 0;
}

// struct vma listを出力
void print_mmap_list(struct task_struct *p, const char *title)
{
    int i = 0;
    debug("[INFO] pid[%d]: vma list (%s) at 0x%lx\n", p->pid, title, p->mm->vmas);

    struct vma *vma = p->mm->vmas;
    while (vma) {
        debug(" - vma[%d]: addr=0x%lx, length=0x%x, prot=0x%x, flags=0x%x, f=%d, offset=0x%x\n",
            ++i, vma->addr, vma->length, vma->prot, vma->flags, (vma->f ? vma->f->ip->inum : 0), vma->offset);
        vma = vma->next;
    }
}

// 親プロセスから子プロセスにvmaをコピー.
int copy_vmas(struct task_struct *parent, struct task_struct *child)
{
    struct vma *node;
    struct vma *cnode = NULL, *tail = 0;

    trace("parent mm: %p, mm->vmas: %p", parent->mm,
            parent->mm->vmas ? parent->mm->vmas : NULL);

    if (!parent->mm || !parent->mm->vmas) {
        trace("%s does not have mm or mm->vmas", parent->name);
        return 0;
    }

    node = parent->mm->vmas;
    //trace("node: %p, addr: 0x%lx, length: 0x%lx", node, node->addr, node->length);
    //trace("parent: %s (%d), child: %s (%d), mm: %p, node: %p", parent->name,
    //        parent->pid, child->name, child->pid, parent->mm, node);

    while (node) {
        struct vma *vma = slab_cache_alloc(VMA);
        if (vma == (struct vma *)0)
            return -ENOMEM;
        trace("vma: %p", vma);
        // vmaのコピー
        copy_vma(vma, node);

        if (cnode == 0)
            cnode = vma;
        else
            tail->next = vma;

        tail = vma;
        node = node->next;
    }

    child->mm->vmas = cnode;

    return 0;
}

/* startを含むvmaを返す */
struct vma *find_vma(struct task_struct *p, uintptr_t start)
{
    struct vma *vma = p->mm->vmas;

    while (vma) {
        if (vma->addr <= start && start < (vma->addr + vma->length))
            return vma;
        vma = vma->next;
    }
    return NULL;
}

/* startを含むvmaを返し、そのprevをセットする */
#if 0
static struct vma *find_vma_prev(struct task_struct *p, uintptr_t start, struct vma **pprev)
{
    struct vma *vma = p->mm->vmas;
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

/* addr + length はp->mm->vmasに含まれるか */
boolean is_vma(struct task_struct *p, uintptr_t addr, uint64_t length)
{
    struct vma *vma = find_vma(p, addr);
    if (vma == NULL)
        return false;
    return (addr + length <= vma->addr + vma->length);
}

#if 0
// 遅延mapを実装（trap.cから呼び出される）
// 該当するアドレスを含む1ページ分の割り当て/ファイル読み込みをする
int alloc_mmap_page(struct task_struct *p, uint64_t addr,  uint64_t scause) {
    off_t offset = 0;
    struct caced_page *cpage;

    uint64_t rounddown = PGROUNDDOWN(addr);
    struct vma *vma = find_vma(p, (void *)rounddown);
    if (vma == NULL) {
        error("no vma with addr 0x%lx", addr);
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
            error("get_page: inum: %d, offset: 0x%x, errno: %ld", vma->f->ip->inum, offset, (int)cpage);
            return -1;
        }
        return uvm_map(p->mm, offset, PGSIZE, (uint64_t)cpage->page, get_perm(vma->prot, vma->flags));
    }
    // ファイルでない場合はページを割り当ててマッピング
    return map_anon_pages(p, rounddown, PGSIZE, get_perm(vma->prot, vma->flags));

    // ファイルオフセットの計算
    if (vma->f) {
        offset = vma->offset + ((rounddown - (uint64_t)vma->addr) / PGSIZE) * PGSIZE;
    }
    trace("pid[%d] addr: 0x%lx, rounddown: 0x%lx, offset: %ld", p->pid, addr, rounddown, offset);
    if (mmap_load_pages((void *)rounddown, PGSIZE, vma->prot, vma->flags, vma->f, offset) < 0) {
        error("loading page failed: addr: 0x%lx, length: 0x%x, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%x",
            rounddown, PGSIZE, vma->prot, vma->flags, vma->f ? vma->f->ip->inum : -1, offset);
        return -1;
    }
    return 0;
}
#endif

// Copy on Write
int copy_vma_pages(uintptr_t addr, size_t length, uint64_t perm)
{
    uint64_t *pte;
    trace("copy_mmap_pages: addr=0x%lx, length=0x%lx, perm=0x%lx\n", addr, length, perm);
    uintptr_t start  = addr;
    for (; start < addr + length; start += PGSIZE) {
        pte = walk(myproc()->mm, start, 0);
        if (pte == 0) { error("copy_mmap_pages: pte = 0\n"); return -EINVAL; }
        uint64_t pa = PTE_ADDR(*pte);
        char *page = kalloc(1);
        if (!page) { error("copy_mmap_pages: no page available\n"); return -ENOMEM; }
        memmove(page, P2V(pa), PGSIZE);
        *pte = V2P(page) | perm;
        trace("- start=0x%lx, pte=0x%lx, *pte=0x%lx\n", start, pte, *pte);
    }
    return 0;
}

#if 0
int alloc_cow_page(uint64_t *pgdir, uint64_t va)
{
    if (va >= MAXVA) {
        error("va: 0x%lx over MAXVA", va);
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
            error("uvm_map error: va=0x%lx", va);
            return -1;
        }
        trace("alloc ok: va=0x%lx, pa: 0x%lx", va, mem);
        fence_i();
        return 0;
    } else if (page_refcnt_get((void *) pa) == 1){
        *pte |= PTE_W;
        *pte &= ~PTE_C;
        trace("flag updated: pa: 0x%lx", pa);
        fence_i();
        return 0;
    } else {
        error("unknown error: va=0x%lx", va);
        return -1;
    }
}
#endif

// musl libc.so対応
//  addr + length がvmaの最高位アドレスに等しい場合に、2つに分割する。
static void *devide_vma(struct vma *vma, uintptr_t addr, size_t length, int prot, int flags, struct file *f, off_t offset)
{
    struct vma *backup;
    int error;

    if (NOT_PAGEALIGN(addr) || NOT_PAGEALIGN(length))
        return MAP_FAILED;

    if ((addr + length) != (vma->addr + vma->length))
        return MAP_FAILED;

    backup = slab_cache_alloc(VMA);
    if (backup == NULL)
        return MAP_FAILED;
    trace("backup: %p", backup);
    memmove((void *)backup, vma, sizeof(struct vma));

    if ((error = munmap(vma->addr, vma->length)) < 0) {
        slab_cache_free(VMA, backup);
        return (void *)(long)error;
    }

    mmap(backup->addr, backup->length - length, backup->prot, (backup->flags | MAP_FIXED), backup->f, backup->offset);
    slab_cache_free(VMA, backup);
    return mmap(addr, length, prot, (flags | MAP_FIXED), f,  offset);
}

static int change_prot(uint64_t start, uint64_t end, int prot)
{
    uint64_t va;
    uint64_t *pte;

    for (va = start; va < end; va+= PGSIZE) {
        pte = walk(myproc()->mm, va, 0);
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
void *mmap(uintptr_t addr, size_t length, int prot, int flags, struct file *f, off_t offset)
{
    struct task_struct *p = myproc();
    struct vma *node, *dev_vma;
    long error = -EINVAL;

    trace("addr: 0x%lx, length: 0x%lx, prot: 0x%x, flags: 0x%x, f: %d, off: 0x%lx",
        addr, length, prot, flags, f ? f->ip->inum : 0, offset);

    // MAP_FIXEDの指定アドレスはページ境界にあり、割り当て領域がMMAPエリア内に入ること
    // 1. addrを確定する
    // 1.1. アドレスが指定されている場合
    if (addr != 0) {
        // 1.1.1 指定アドレス+指定サイズはUSER_TOPを超えないこと
        // upper_addr: addr + sizeがMMAPTOPを超えないかチェックするための変数
        uint64_t upper_addr = PGROUNDUP(PGROUNDUP(addr) + length);
        if (upper_addr > USER_TOP) {
            error("addr: 0x%lx + length: 0x%lx overs USER_TOP", addr, length);
            return (void *)-EINVAL;
        }
        // 1.1.2 MAP_FIXEDが指定されている場合
        if (flags & MAP_FIXED) {
            // 1.1.2.1 アドレスはページ境界にあること
            if (NOT_PAGEALIGN(addr)) {
                error("fixed address should be page align: 0x%lx", addr);
                return (void *)-EINVAL;
            }
            // ユーザアクセス禁止のpteを作成: mappingなし
            if (prot == PROT_NONE) {
                for (int i = 0; i < PGROUNDUP(length) / PGSIZE; i++) {
                    uint64_t *pte = walk(p->mm, addr + i * PGSIZE, 1);
                    if (pte == NULL) {
                        error("PROT_NONE invalid addr");
                        return (void *)-EINVAL;
                    }
                    // ユーザアクセスを禁止にする
                    *pte &= ~PTE_USER;
                }
                return (void *)addr;
            } else {
                // 1.1.2.2 指定されたアドレスが使用可能なこと
                dev_vma = is_usable(addr, length);
                if (dev_vma == MAP_FAILED) {
                    error("addr 0x%lx is not available", addr);
                    return (void *)-EINVAL;
                } else if (dev_vma == NULL) {
                    /* ok */
                } else {
                    return devide_vma(dev_vma, addr, length, prot, flags, f, offset);
                }
            }
        // 1.1.3 MAP_FIXEDが指定されていない場合は、アドレスを丸め下げ
        } else {
            addr = PGROUNDDOWN(addr);
            goto select_addr;
        }
    // 1.2. アドレスが指定されていない場合
    } else {
        // 1.2.1 最初のアドレス候補
        if (p->mm && p->mm->vmas && (uint64_t)p->mm->vmas->addr >= MMAP_BASE)
            addr = p->mm->vmas->addr;
        else
            addr = MMAP_BASE;
select_addr:
        node = p->mm ? p->mm->vmas : NULL;
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
    if (addr + PGROUNDUP(length) > USER_TOP)
        return (void *)-ENOMEM;
    trace("select addr: 0x%lx", addr);
    // 2. 新規vmaを作成する
    // 2.1 vmaのためのメモリを割り当てる
    struct vma *vma = slab_cache_alloc(VMA);
    if (vma == NULL)
        return (void *)-ENOMEM;
    trace("vma: %p", vma);
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

    // 3. p->mm->vmasに作成したvmaを追加する
    // 3.1 これがプロセスの最初のvmaの場合はp->mm->vmasに追加する
    if (!p->mm) {
        panic("%s does not have mm", p->name);
    }
    if (p->mm->vmas == NULL) {
        trace("first vma: addr: 0x%lx", vma->addr);
        p->mm->vmas = vma;
        goto load_pages;
    }

    // 3.2 そうでない場合は適切な位置に追加して、p->mm->vmasを更新する
    node = p->mm->vmas;
    struct vma *prev = p->mm->vmas;
    while (node) {
        trace("update vmas: addr=0x%lx, node->addr=0x%lx\n", addr, node->addr);
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
    p->mm->vmas = prev;

load_pages:

    if ((error = mmap_load_pages(addr, length, prot, flags, f, offset)) < 0) {
        error("mmap_load_pages: error: %d, addr: 0x%lx, length: 0x%lx, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%lx", error, addr, length, prot, flags, f ? f->ip->inum : -1, offset);
        goto out;
    }

    vma->addr = addr;
    // ファイルオフセットを正しく処理するためにlengthはここで切り上げる
    vma->length = PGROUNDUP(length);

    trace("return addr: 0x%lx, length: 0x%lx, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%x",
        vma->addr, vma->length, vma->prot, vma->flags, vma->f ? vma->f->ip->inum : 0, vma->offset);
    //print_mmap_list(p, "mmap");

    return (void *)vma->addr;

out:
    if (f) fileclose(f);
    slab_cache_free(VMA, vma);
    return (void *)error;
}

// sys_munmapのメイン関数
int munmap(uint64_t addr, size_t length)
{
    struct task_struct *p = myproc();

    trace("called addr: 0x%lx, length: 0x%lx", addr, length);

    // addrはページ境界になければならない
    if (NOT_PAGEALIGN(addr))
        return -EINVAL;

    // lengthは境界になくてもよいが、処理は境界に合わせる
    length = PGROUNDUP(length);

    trace("pid[%d] addr=0x%lx, length=0x%x", p->pid, addr, length);

    struct vma *vma = find_vma(p, addr);
    if (vma == NULL) {
        trace("no vma with addr 0x%lx", addr);
        return 0;
    }

    if (vma->length < length) {
        trace("length 0x%lx is bigger than vma->length 0x%lx", length, vma->length);
        length = vma->length;
    }

    trace(" - found: addr=0x%lx, length=0x%x", vma->addr, vma->length);
    trace(" - delete 0x%lx bytes", length);

    // MAP_SHARED領域で背後にあるファイルに書き込みがあったら書き戻す
    //int len = (f->vnode->size - f->offset) > PGSIZE ? PGSIZE : (f->vnode->size - f->offset);
    if (vma->flags & MAP_SHARED && vma->f && vma->prot & PROT_WRITE) {
        for (uint64_t ra = addr; ra < addr + length; ra += PGSIZE) {
            //uint64_t *pte = walk(p->mm, (void *)ra, 0);
            //if (!pte) panic("no pte");
            //uint64_t flags = PTE_FLAGS(*pte);
            off_t offset = vma->offset + ra - vma->addr;
            trace("writeback: f: %d, offset: 0x%x, ra: 0x%lx", vma->f->ip->inum, offset, ra);
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
        uvm_unmap(p->mm, addr, length / PGSIZE, 1);
        if (vma->addr == addr) {
            vma->addr += length;
            vma->offset += length;
        }
        vma->length -= length;
        trace("new vma: addr=0x%lx, length=0x%x", vma->addr, vma->length);
    }
    //print_mmap_list(p, "munmap");
    //vm_stat(p->mm);

    return 0;
}

void *mremap(uintptr_t old_addr, size_t old_length, size_t new_length, int flags, uintptr_t new_addr)
{
    void *mapped_addr;
    long error = -EINVAL;
    //trace("- remap: old_addr=0x%lx, old_length=0x%x, new_length=0x%x, flags=0x%x",
    //    old_addr, old_length, new_addr, flags);
    struct vma *vma = find_vma(myproc(), old_addr);
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
            //uvm_switch(myproc()->pagetable);
            return (void *)vma->addr;
        }
    }
    // 2: その場では拡大できない場合
    if (!(flags & MREMAP_MAYMOVE)) return (void *)error;

    if (flags & MREMAP_FIXED)
        mapped_addr = mmap(new_addr, new_length, vma->prot, (vma->flags | MAP_FIXED), vma->f, vma->offset);
    else
        mapped_addr = mmap(new_addr, new_length, vma->prot, (vma->flags & ~MAP_FIXED), vma->f, vma->offset);

    if (IS_ERR(mapped_addr)) {
        return (void *)mapped_addr;

    }

    memmove((char *)new_addr, (const char *)vma->addr, vma->length);
    if ((error = msync((uintptr_t)mapped_addr, new_length, MS_SYNC)) < 0)
        return (void *)error;

    if ((error = munmap(vma->addr, vma->length)) < 0)
        return (void *)error;
    //uvm_switch(myproc()->pagetable);
    //print_mmap_list(myproc(), "mremap");
    return mapped_addr;
}

int mprotect(uintptr_t addr, size_t length, int prot)
{
    struct task_struct *p = myproc();
    struct vma *vma, *new1, *new2;
    int newprot;

    trace("addr: 0x%lx, length: 0x%lx, prot: 0x%x", addr, length, prot);

    uintptr_t addrp = addr;
    if (addrp <= p->mm->sz) {
        return change_prot(addrp, addrp + length, prot);
    }

    if (!is_vma(p, addr, length)) {
        error("invalid vma: addr: 0x%lx, length: 0x%lx", addr, length);
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
        trace("new2: %p", new2);
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
        trace("new1: %p", new1);
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
        trace("new1: %p", new1);
        new2 = slab_cache_alloc(VMA);
        if (!new2) return -ENOMEM;
        trace("new2: %p", new2);
        new1->addr = vma->addr;
        new1->length = addr - vma->addr;
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

    return change_prot(vma->addr, vma->addr + vma->length, prot);

    //    print_mmap_list(p, "mprotect");
}

int msync(uintptr_t addr, size_t length, int flags)
{
    struct task_struct *p = myproc();
    int error = -EINVAL;

    if (!(flags & MS_ASYNC) && !(flags & MS_SYNC)) {
        flags |= MS_ASYNC;
    }

    if (flags & MS_ASYNC) {
        // Since  Linux  2.6.19, MS_ASYNC  is  in  fact  a no-op (from man(2))
        return 0;
    }

    // addrはページ境界になければならない。
    if (NOT_PAGEALIGN(addr)) return error;

    struct vma *vma = find_vma(p, addr);
    if (vma == NULL) {
        trace("no vma with addr 0x%lx", addr);
        return 0;
    }

    if (vma->length < length) {
        trace("length 0x%lx is bigger than vma->length 0x%lx", length, vma->length);
        length = vma->length;
    }

    // ファイルが背後にあり共有されているマッピングで変更があった場合は書き戻す
    if (vma->flags & MAP_SHARED && vma->f && vma->prot & PROT_WRITE) {
        for (uintptr_t ra = addr; ra < addr + length; ra += PGSIZE) {
            uint64_t *pte = walk(p->mm, ra, 0);
            if (!pte) panic("mysync: no pte");
            off_t offset = vma->offset + ra - vma->addr;
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
