#include <types.h>
#include <mmu.h>
#include <sched.h>
#include <string.h>
#include <memlayout.h>
#include <console.h>
#include <vm.h>
#include <mm.h>
#include <stacktrace.h>

#if 0
/*
 * If the page is invalid, then allocate a new one. Return NULL if failed.
 */
static uint64_t *pde_validate(uint64_t *pde, int64_t alloc)
{
    if (!(*pde & PTE_P)) {  // if the page is invalid
        if (!alloc) return NULL;
        char *p = kalloc(1);
        if (!p) return NULL;  // allocation failed
        memset(p, 0, PAGE_SIZE);
        *pde = V2P(p) | PTE_P | PTE_PAGE | PTE_USER | PTE_RW;
    }
    return pde;
}

/*
 * Given 'pgdir', a pointer to a page directory, pgdir_walk returns
 * a pointer to the page table entry (PTE) for virtual address 'va'.
 * This requires walking the four-level page table structure.
 *
 * The relevant page table page might not exist yet.
 * If this is true, and alloc == false, then pgdir_walk returns NULL.
 * Otherwise, pgdir_walk allocates a new page table page with kalloc.
 *   - If the allocation fails, pgdir_walk returns NULL.
 *   - Otherwise, the new page is cleared, and pgdir_walk returns
 *     a pointer into the new page table page.
 */

static uint64_t *pgdir_walk(uint64_t *pgdir, const void *va, int64_t alloc)
{
    uint64_t *pde = pgdir;

    for (int l = 0; l < 3; l++) {
        pde = &pde[PTX(l, va)];
        if (!(pde = pde_validate(pde, alloc)))
            return NULL;
        pde = (uint64_t *)P2V(PTE_ADDR(*pde));
    }
    return &pde[L3X(va)];
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might **NOT**
 * be page-aligned.
 * Use permission bits perm|PTE_P|PTE_TABLE|PTE_AF for the entries.
 *
 * Hint: call pgdir_walk to get the corresponding page table entry
 */

static int map_region(uint64_t *pgdir, void *va, uint64_t size, uint64_t pa, int64_t perm)
{
    void *vap = (void *)PGROUNDDOWN((uint64_t)va);
    void *end = va + size;
    pa = PGROUNDDOWN(pa);

    for (; vap < end; pa += PAGE_SIZE, vap += PAGE_SIZE) {
        uint64_t *pte = pgdir_walk(pgdir, vap, 1);
        if (!pte) {
            cprintf("walk failed\n");
            return 1;
        }
        if (*pte & PTE_P)
            panic("remap");
        *pte = pa | perm | PTE_P | PTE_TABLE | (MT_NORMAL << 2) | PTE_AF | PTE_SH;
    }

    return 0;
}

/*
 * Free a page table.
 *
 * Hint: You need to free all existing PTEs for this pgdir.
 */

void vm_free(uint64_t *pgdir, int level)
{
    if (!pgdir || level < 0 || level > 3)
        return;
    if (PTE_FLAGS(pgdir))
        panic("vm_free: invalid pgdir\n");
    if (level == 0) {
        kfree((char *)pgdir);
        return;
    }
    for (uint64_t i = 0; i < ENTRYSZ; i++) {
        if (pgdir[i] & PTE_P) {
            uint64_t *v = (uint64_t *)P2V(PTE_ADDR(pgdir[i]));
            vm_free(v, level - 1);
        }
    }
    kfree((char *)pgdir);
}
#endif

// 1ページ割り当て、userにマッピングする. ページのカーネル仮想アドレスを返す, 0 if failed.
void *allocate_user_page_mm(struct mm_struct *mm, uint64_t va, uint64_t perm)
{
    uint64_t page;

    if (mm->user_pages_count == MAX_TASK_USER_PAGES) {
        error("reached limit of MAX_TASK_USER_PAGES %d", MAX_TASK_USER_PAGES);
        return 0;
    }

    page = get_free_page();    // このpageはpa
    if (page == 0) {
        error("no page");
        return 0;
    }
    trace("call map_page");
    trace("va: 0x%lx, page: 0x%lx, alloc: 1, perm: 0x%lx", va, page, perm);
    if (map_page(mm, va, page, 1, perm)) {
        return P2V(page);       // 仮想アドレスを返す
    } else {
        error("map_page failed: va=0x%lx, pa=0x%lx, perm=0x%lx", va, page, perm);
        return 0;
    }
}

static uint64_t *map_table_entry(uint64_t *pte, uint64_t va, uint64_t pa, uint64_t perm)
{
    uint64_t index = va >> PAGE_SHIFT;
    index = index & (PTRS_PER_TABLE - 1);
    if (pa) {
        uint64_t entry = pa | MMU_PTE_FLAGS | perm;
        pte[index] = entry;
    }
    return pte + index;
}

// 仮想アドレスvaからテーブルインデックスを抽出し、親テーブルのディスクリプタを用意する。
// 次のレベルのpgtableの物理アドレスを返す
static uint64_t map_table(uint64_t *table /*va*/, uint64_t shift, uint64_t va,
    int *alloc, uint64_t **pdesc)
{
    uint64_t index = (va >> shift);
    index = index & (PTRS_PER_TABLE - 1);
    trace("va: 0x%lx, shift: %d, table[%d]: 0x%lx", va, shift, index, table[index]);
    if (pdesc) {
        *pdesc = table + index;
    }
    if (!table[index]) {
        if (*alloc) {
            uint64_t next_level_table = get_free_page();    // pa
            if (!next_level_table) {
                error("no more memory");
                *alloc = 0;
                return 0;
            }
            uint64_t entry = next_level_table | MM_TYPE_PAGE_TABLE;
            table[index] = entry;
            trace("table[%d]: 0x%lx, next_table: 0x%lx", index, table[index], next_level_table);
            return next_level_table;
        } else {
            *alloc = 0;
            return 0;
        }
    } else {
        *alloc = 0;
        trace("PTE: 0x%lx, pa: 0x%lx", table[index], PTE_TO_PA(table[index]));
        return PTE_TO_PA(table[index]);
    }
}

//タスクのpgtableツリーをたどる. ユーザ仮想アドレスに対応するpteを探して（オプションで）更新する。
// pteのカーネル仮想アドレスを返す
uint64_t *map_page(struct mm_struct *mm, uint64_t va /*uva*/, uint64_t page /*pa*/, int alloc, uint64_t perm)
{
    uint64_t *desc;
    assert(mm);
    trace("va: 0x%lx, page: 0x%lx, alloc: %d, perm: 0x%lx", va, page, alloc, perm);
    int nk = mm->kernel_pages_count;
    uint64_t *descs[4] = { 0 };
    uint64_t ker_pages[4] = { 0 };      // pa

    if (page != 0 && alloc == 1 && mm->user_pages_count >= MAX_TASK_USER_PAGES) {
        error("no more user pages: user_pages_count: %d", mm->user_pages_count);
        return 0;
    }

    // タスクのトップレベルのpgtableから開始する。ない場合は割り当てる
    if (!mm->pgd) {
        if (alloc) {
            if (nk == MAX_TASK_KER_PAGES)
                goto fail;
            if (!(mm->pgd = get_free_page()))
                goto fail;
            ker_pages[0] = mm->pgd;
            descs[0] = &(mm->pgd);
        } else {
            goto fail;
        }
    }
    trace("[0] mm->pgd: 0x%lx, kpage: 0x%lx, descs: %p (0x%lx)", mm->pgd, ker_pages[0], descs[0], *descs[0]);

    __attribute__((unused)) const char *lvs[] = { "pgd", "pud", "pmd", "pte" };
    const int shifts[] = { 0, PGD_SHIFT, PUD_SHIFT, PMD_SHIFT };
    uint64_t table = mm->pgd;       // pgd/pud/pmd/pteのpa
    int allocated;

    for (int i = 1; i < 4; i++) {
        allocated = alloc;
        trace("before[%d] table: pa(0x%lx), va(0x%lx)", i, table, P2V(table));
        table = map_table(P2V(table), shifts[i], va, &allocated, &desc);
        trace("[%d] table: 0x%lx, allocated: %ld, desc: %p", i, table, allocated, desc);
        if (table) {
            if (allocated) {
                ker_pages[i] = table;
                descs[i] = desc;
                if (nk + i > MAX_TASK_KER_PAGES) {
                    error("MAX_TASK_KER_PAGES %d reached", MAX_TASK_KER_PAGES);
                    goto fail;
                }
            } else {
                // noop
            }
        } else {
            if (!alloc) {
                warn("%s: we reached nonexisting pgtable, and asked not to alloc", lvs[i]);
            } else {
                warn("%s: asked to alloc but still failed. low kernel mem?", lvs[i]);
            }
            goto fail;
        }
        trace("[%d] kpage: 0x%lx, descs: %p (0x%lx)", i, ker_pages[i], descs[i], *descs[i]);
    }

    uint64_t *pte_va = map_table_entry(P2V(table), va, page, perm);
    if (page) {
        assert(mm->user_pages_count < MAX_TASK_USER_PAGES);
        mm->user_pages_count++;
    }

    for (int i = 0; i < 4; i++) {
        if (ker_pages[i] == 0) continue;
        mm->kernel_pages[mm->kernel_pages_count++] = ker_pages[i];
    }
    trace("pte_va: %p (0x%lx)", pte_va, *pte_va);
    return pte_va;

fail:
    for (int i = 0; i < 4; i++) {
        if (ker_pages[i] == 0) continue;
        assert(alloc && descs[i]);
        kfree((char *)P2V(ker_pages[i]));
        *descs = 0;
    }
    error("failed. reverse allocated pgtables during tree walk");
    return 0;
}

// カレントタスクのユーザページをdstタスクにコピーする
int dup_current_virt_memory(struct mm_struct *dstmm)
{
    struct trapframe *regs = task_pt_regs(myproc());
    struct mm_struct *srcmm = myproc()->mm;
    assert(srcmm);

    acquire(&srcmm->lock);
    for (uint64_t i = 0; i < srcmm->sz; i += PAGE_SIZE) {
        trace("call map_page");
        uint64_t *pte = map_page(srcmm, i, 0, 0, 0);
        assert(pte);
        uint64_t perm = PTE_TO_PERM(*pte);
        void *kernel_va = allocate_user_page_mm(dstmm, i, perm);
        if (kernel_va == 0)
            goto no_mem;
        memmove(kernel_va, (void *)i, PAGE_SIZE);
    }

    // ユーザスタックをコピーする
    for (uint64_t i = PGROUNDDOWN(regs->sp); i < USER_TOP; i += PAGE_SIZE) {
        trace("call map_page");
        uint64_t *pte = map_page(srcmm, i, 0, 0, 0);
        assert(pte);
        void *kernel_va = allocate_user_page_mm(dstmm, i, MM_AP_RW | MM_XN);
        if (kernel_va == 0)
            goto no_mem;
        memmove(kernel_va, (void *)i, PAGE_SIZE);
    }

    dstmm->sz = srcmm->sz;
    dstmm->codesz = srcmm->codesz;

    release(&srcmm->lock);
    return 0;

no_mem:
    release(&srcmm->lock);
    error("no more memory");
    return -1;
}

// 仮想アドレスの物理アドレスを返す
uint64_t walkaddr(struct mm_struct *mm, uint64_t va)
{
    trace("va: 0x%lx", va);
    uint64_t *pte = map_page(mm, va, 0, 0, 0);
    if (!pte) return 0;

    if ((*pte & ~(PAGE_MASK) & (~(uint64_t)MM_AP_MASK)) != MMU_PTE_FLAGS) {
        error("pte (*pte: 0x%lx) found but invalid %016lx, %016lx", *pte, (*pte & ~(PAGE_MASK) & (~(uint64_t)MM_AP_MASK)), MMU_PTE_FLAGS);
        return 0;
    }
    trace("va: 0x%lx, *pte: 0x%lx, pte_pa: 0x%lx", va, *pte, PTE_TO_PA(*pte));
    assert(PTE_TO_PA(*pte) >= USER_BASE && PTE_TO_PA(*pte) < USER_TOP);
    trace("va: 0x%lx, pa: 0x%lx", va, PTE_TO_PA(*pte));
    return PTE_TO_PA(*pte);
}

// カーネルからユーザへコピーする
int copyout(struct mm_struct *mm, uint64_t dstva, char *src, uint64_t len)
{
    uint64_t n, va0, pa0;

    if (dstva > USER_TOP) {
        error("illegal user va. a kernel va??");
        return -1;
    }

    acquire(&mm->lock);
    while (len > 0) {
        va0 = PGROUNDDOWN(dstva);
        pa0 = walkaddr(mm, va0);
        if (pa0 == 0) {
            release(&mm->lock);
            return -1;
        }
        n = PAGE_SIZE - (dstva - va0);
        if (n > len) n = len;
        memmove(P2V(pa0 + (dstva - va0)), src, n);

        len -= n;
        src += n;
        dstva = va0 + PAGE_SIZE;
    }
    release(&mm->lock);

    return 0;
}

// ユーザからカーネルにコピーする
int copyin(struct mm_struct *mm, char *dst, uint64_t srcva, uint64_t len)
{
   uint64_t n, va0, pa0;

    if (srcva > USER_TOP) {
        error("illegal user va. a kernel va??");
        return -1;
    }

    acquire(&mm->lock);
    while (len > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(mm, va0);
        if (pa0 == 0) {
            release(&mm->lock);
            return -1;
        }
        n = PAGE_SIZE - (srcva - va0);
        if (n > len) n = len;
        memmove(dst, P2V(pa0 + (srcva - va0)), n);

        len -= n;
        dst += n;
        srcva = va0 + PAGE_SIZE;
    }
    release(&mm->lock);

    return 0;
}

// NULL終端の文字列をユーザからカーネルにコピーする
int copyinstr(struct mm_struct *mm, char *dst, uint64_t srcva, uint64_t max)
{
    uint64_t n, va0, pa0;
    int got_null = 0;

    if (srcva > USER_TOP) {
        error("illegal user va. a kernel va??");
        return -1;
    }

    acquire(&mm->lock);
    while (got_null == 0 && max > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(mm, va0);
        if (pa0 == 0) {
            release(&mm->lock);
            return -1;
        }
        n = PAGE_SIZE - (srcva - va0);
        if (n > max) n = max;

        char *p = (char *)P2V(pa0 + (srcva - va0));
        while (n > 0) {
            if (*p == '\0') {
                *dst = '\0';
                got_null = 1;
                break;
            } else {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
        }
        srcva = va0 + PAGE_SIZE;
    }
    release(&mm->lock);

    return got_null ? 0 : -1;
}

// usr_dstに基づいて、ユーザアドレスまたはカーネルアドレスのいずれかにコピーする
int either_copyout(int user_dst, uint64_t dst, void *src, uint64_t len)
{
    struct task_struct *p = myproc();

    if (user_dst) {
        return copyout(p->mm, dst, src, len);
    } else {
        memmove((char *)dst, src, len);
        return 0;
    }
}

// usr_dstに基づいて、ユーザアドレスまたはカーネルアドレスのいずれかからコピーする
int either_copyin(void *dst, int user_src, uint64_t src, uint64_t len)
{
    struct task_struct *p = myproc();

    if (user_src) {
        return copyin(p->mm, dst, src, len);
    } else {
        memmove(dst, (char *)src, len);
        return 0;
    }
}

// useronly=1の場合はユーザページのみを、それ以外はユーザ／カーネルページを解放する
void free_task_pages(struct mm_struct *mm, int useronly)
{
    uint64_t page, sz;
    assert(mm);

    sz = mm->sz;
    if (growproc(mm, -sz) == (uint64_t)(void *)-1){
        error("faild free user pages");
        return ;
    }

    if (!useronly) {
        for (int i = 0; i < mm->kernel_pages_count; i++) {
            page = mm->kernel_pages[i];
            assert(page);
            kfree((char *)P2V(page));
        }
        memset(&mm->kernel_pages, 0, sizeof(mm->kernel_pages));
        mm->kernel_pages_count = 0;
        mm->pgd = 0;
    }
}

// ユーザページをアンマップして解放する
static int free_user_page_range(struct mm_struct *mm, uint64_t start_va, uint64_t size)
{
    uint64_t *pte;
    uint64_t page;      // pa
    int cnt = 0;
    trace("va: 0x%lx, size: 0x%lx", start_va, size);

    assert(!(start_va & (~PAGE_MASK)) && !(size & (~PAGE_MASK)) && mm);

    for (uint64_t i = start_va; i < start_va + size; i += PAGE_SIZE, cnt++) {
        trace("call map_page");
        pte = map_page (mm, i, 0, 0, 0);
        if (!pte) {
            error("map_page failed: i: 0x%lx, cnt: %d", i, cnt);
            return -1;
        }
        page = PTE_TO_PA(*pte);
        kfree((char *)P2V(page));
        assert(mm->user_pages_count > 0);
        mm->user_pages_count--;
        *pte = 0;
    }
    return cnt;
}

// szを増減する
uint64_t growproc(struct mm_struct *mm, int incr)
{
    uint64_t sz = mm->sz, sz1;
    void *kva;
    int ret;
    trace("sz: 0x%lx, incr: 0x%x", sz, incr);

    if (incr < 0 && sz < -incr) {
        error("incr too small: sz 0x%lx (%ld) incr %d. requested new brk 0x%lx",
            sz, sz, incr, sz + incr);
        goto bad;
    }

    if (incr > 0 && sz + incr >= USER_TOP) {
        error("incr too large: sz 0x%lx (%ld), incr %d. req new brk 0x%lx",
            sz, sz, incr, sz + incr);
        goto bad;
    }

    if (incr >= 0) {
        for (sz1 = PGROUNDUP(sz); sz1 < sz + incr; sz1 += PAGE_SIZE) {
            kva = allocate_user_page_mm(mm, sz1, MM_AP_RW | MM_XN);
            if (!kva) {
                error("allocate_user_page_mm failed");
                goto reverse;
            }
        }
    } else {
        int ret = free_user_page_range(mm, PGROUNDUP(sz + incr),
                    PGROUNDDOWN(sz) - PGROUNDUP(sz + incr));
        assert(ret != -1);
    }
    sz1 = mm->sz;
    mm->sz += incr;
    if (myproc()->mm == mm)
        set_pgd(mm->pgd);

    return sz1;

reverse:
    ret = free_user_page_range(mm, PGROUNDUP(sz),sz1 - PGROUNDUP(sz));
    assert(ret != -1);
bad:
    return (uint64_t)(void *)-1;
}

static int ind = 1;
int do_mem_abort(uint64_t addr, uint64_t esr, uint64_t elr)
{
    struct task_struct *p = myproc();
    __attribute__((unused)) struct trapframe *regs = task_pt_regs(p);
    uint64_t dfs = (esr & 0b111111);
    debug("addr: 0x%lx, dfs: 0x%lx", addr, dfs);
    if (addr > USER_TOP) {
        error("bad user va? faulty addr 0x%lx > USER_TOP 0x%x\n"
              "esr 0x%lx, elr 0x%lx", addr,USER_TOP, esr, elr);
        goto bad;
    }
    if ((dfs & 0b111100) == 0b100) {            // デマンドページング処理
        uint64_t page = get_free_page();
        if (page == 0) {
            error("no more memory");
            goto bad;
        }
        acquire(&p->mm->lock);
        trace("call map_page");
        map_page(p->mm, addr & PAGE_MASK, page, 1, MMU_PTE_FLAGS | MM_AP_RW);
        release(&p->mm->lock);
        ind++;
        if (ind >5) {
            error("addr: 0x%lx, pid %d too many mem faults. ind %d. killed",
                    addr, myproc()->pid, ind);
            goto bad;
        }
        debug("demand paging at user va 0x%lx, elr 0x%lx ok", addr, regs->pc);
        ind = 0;
        return 0;
    }
    error("cannot handle: not translation fault.\nFAR 0x%016lx\nESR 0x%08lx\nELR 0x%016lx",
            addr, esr, elr);
    error("online esr decoder: %s0x%016lx", "https://esr.arm64.dev/#", esr);
    //debug_hexdump((void *)elr, 32);

bad:
    show_stack_user();
    setkilled(p);
    return 0;
}

