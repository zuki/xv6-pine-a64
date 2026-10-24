#include <types.h>
#include <exec.h>
#include <sched.h>
#include <mmu.h>
#include <elf.h>
#include <file.h>
#include <console.h>
#include <mm.h>
#include <vm.h>
#include <linux/auxvec.h>

// fxl: derived from xv6, for which user stack NOT at the very end of va
//  but next page boundary of user code (with 1 pg of stack guard
//  in between)   this limits to user stack to 4KB. (not growing)
//  (why designed like this??)
static int loadseg(struct mm_struct *mm, uint64_t va, struct inode *ip,
  uint32_t offset, uint32_t sz);

static int flags2perm(int flags)
{
    assert(flags & PF_R);

    int perm = MMU_PTE_FLAGS;

    if (flags & PF_W)
      perm |= MM_AP_RW;
    else
      perm |= MM_AP_RO;

    if (!(flags & PF_X))
      perm |= MM_XN;

    return perm;
}


// Called from sys_exec
// for now, no support for exec() on fat fs
int exec(char *path, char *argv[], int argcc, char *envp[], int envcc)
{
    char *s, *last;
    int i, off;
    uint64_t argc = 0, envc = 0;
    uint64_t sz = 0, sz1, sp, ustack[MAXARG], estack[MAXARG], argbase;
    struct mm_struct *tmpmm = 0;  // new mm to prep aside, and commit to user VM
    Elf64_Ehdr elf;
    struct inode *ip;
    Elf64_Phdr ph;
    void *kva;
    struct task_struct *p = myproc();

    debug("pid: %d, path: %s, argv: %p, envp: %p, argc: %d, envc: %d", myproc()->pid, path, argv, envp, argcc, envcc);

    if ((ip = namei(path, AT_FDCWD)) == 0){
        return -1;
    }
    ilock(ip);

    // tmpmm->lockのロックは不要 -- tmpmmはプライベートだから
    // acquire(&p->mm->lock);

    if (readi(ip, 0, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf)) // ELF header good?
        goto bad;

    trace("ELF: entry: 0x%lx, phoff: 0x%lx, phnum: 0x%lx", elf.e_entry, elf.e_phoff, elf.e_phnum);

    if (ELF_CHECK_E_IDENT(&elf) == 0) // ELF magic good?
        goto bad;

    if (p->mm->pgd == 0) // タスクは有効なpgtableツリーを持っていなければならない
        goto bad;   // XXXX race condition; need lock

    // 簡単のため、mm_structに1ページ割り当てる
    _Static_assert(sizeof(struct mm_struct) <= PAGE_SIZE);
    tmpmm = kalloc(1);
    assert(tmpmm);

    initlock(&tmpmm->lock, "tmpmmlock");

    tmpmm->kernel_pages_count = p->mm->kernel_pages_count;
    memmove(&tmpmm->kernel_pages, &p->mm->kernel_pages, sizeof(tmpmm->kernel_pages));
    tmpmm->pgd = 0; // 新規pgtableツリーでスタートする

    // プログラムをtmpmmのメモリにロードする: すべてのプログラムヘッダーについてイテレートする
    // 注: 以下のコードはp_vaddrが昇順になっていることを仮定している
    for (i=0, off=elf.e_phoff; i<elf.e_phnum; i++, off+=sizeof(ph)){
        // プログラムヘッダーを1つ読み込む
        if (readi(ip, 0, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;
        if (ph.p_type != PT_LOAD)
            continue;
        trace("ph[%d]: pid: %d, vaddr: 0x%lx, fsz: 0x%lx, msz: 0x%x, sz: 0x%lx", i, myproc()->pid, ph.p_vaddr, ph.p_filesz, ph.p_memsz, sz);
        assert(ph.p_vaddr >= sz); // sz: last seg's va end (exclusive)
        if (ph.p_memsz < ph.p_filesz)  // memsz: seg size in mem; filesz: seg bytes to load from file
            goto bad;
        // ページヘッダーの開始アドレスはページアラインでなければならないが、終了アドレスはそうでなくても良い
        if (ph.p_vaddr + ph.p_memsz < ph.p_vaddr)
            goto bad;
        if (i == 0 && ph.p_vaddr % PAGE_SIZE != 0)
            goto bad;
        // コード用にメモリを割り当てる
        sz1 = PGROUNDDOWN(ph.p_vaddr);
        for (; sz1 < ph.p_vaddr + ph.p_memsz; sz1 += PAGE_SIZE) {
            kva = allocate_user_page_mm(tmpmm, sz1, flags2perm(ph.p_flags));
            assert(kva);
        }
        sz = sz1;
        // コードをロードする
        if (loadseg(tmpmm, ph.p_vaddr, ip, ph.p_offset, ph.p_filesz) < 0)
            goto bad;
    }
    iunlockput(ip);
    ip = 0;

    trace("pid %d done loading prog", myproc()->pid);

    sz = PGROUNDUP(sz);

    /* ユーザスタックを新規に割り当てる  */
    assert(sz + PAGE_SIZE + USER_MAX_STACK <= USER_TOP);
    // 引数をロードするために、スタックページの最初の1ページを割り当てる（これはデマンドページを行わない）
    if (!(kva=allocate_user_page_mm(tmpmm, USER_TOP - PAGE_SIZE, MMU_PTE_FLAGS | MM_AP_RW))) {
        goto bad;
    }
    memzero_aligned(kva, PAGE_SIZE);
    // USER_MAX_STACKの下にガードページを置く
    if (!(kva=allocate_user_page_mm(tmpmm, USER_TOP - USER_MAX_STACK - PAGE_SIZE, MMU_PTE_FLAGS | MM_AP_EL1_RW))) {
        goto bad;
    }

    /* ユーザスタックにプログラム引数を用意する  */
    sp = USER_TOP;
    argbase = USER_TOP - PAGE_SIZE; // 引数と環境変数で1ページ
    // Push argument strings, prepare rest of stack in ustack.
    // Populate arg strings on stack. save arg pointers to ustack (a scratch buf)
    //    then populate "ustack" on the stack

    if (argcc > 0) {
        for (argc = 0; argv[argc]; argc++) {
            if (argc >= MAXARG)
                goto bad;
            sp -= strlen(argv[argc]) + 1;
            sp -= sp % 16;
            if (sp < argbase)       // 引数の合計が1ページを超えたらエラー
                goto bad;
            if (copyout(tmpmm, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
                goto bad;
            ustack[argc] = sp;
        }
        ustack[argc] = 0; // last arg
    } else {
        ustack[0] = 0;
    }

    if (envcc > 0) {
        for (envc = 0; envp[envc]; envc++) {
            if (envc >= MAXARG)
                goto bad;
            sp -= strlen(envp[envc]) + 1;
            sp -= sp % 16;
            if (sp < argbase)       // 引数と環境変数の合計が1ページを超えたらエラー
                goto bad;
            if (copyout(tmpmm, sp, envp[envc], strlen(envp[envc]) + 1) < 0)
                goto bad;
            estack[envc] = sp;
        }
        estack[envc] = 0; // last arg
    } else {
        estack[0] = 0;
    }

    uint64_t auxv[][2] = { {AT_PAGESZ, PGSIZE}, {AT_NULL, 0} };
    uint64_t auxv_size = sizeof(auxv);

    sp -= 4 * sizeof(uint64_t);
    sp -= sp % 16;
    if (sp < argbase)
        goto bad;
    if (copyout(tmpmm, sp, (char *)auxv, 4*sizeof(uint64_t)) < 0)
        goto bad;

    // push the array of argv[] pointers.
    sp -= (argc+1) * sizeof(uint64_t);
    sp -= sp % 16;  // fxl: do we need this
    if (sp < argbase)
        goto bad;
    if (copyout(tmpmm, sp, (char *)ustack, (argc+1)*sizeof(uint64_t)) < 0)
        goto bad;

    // push the array of envp[] pointers.
    sp -= (envc+1) * sizeof(uint64_t);
    sp -= sp % 16;  // fxl: do we need this
    if (sp < argbase)
        goto bad;
    if (copyout(tmpmm, sp, (char *)estack, (envc+1)*sizeof(uint64_t)) < 0)
        goto bad;

    sp -= sizeof(uint64_t);
    sp -= sp % 16;
    if (sp < argbase)
        goto bad;
    if (copyout(tmpmm, sp, (char *)&argc, sizeof(uint64_t)) < 0)
        goto bad;


    // arguments to user main(argc, argv)
    // argc is returned via the system call return value, which goes in r0.
    struct trapframe *regs = task_pt_regs(p);
    regs->regs[1] = sp;

    // Save program name for debugging.
    for (last = s = path; *s; s++)
        if (*s == '/')
            last = s+1;
    safestrcpy(p->name, last, sizeof(p->name));

    /* Commit to the user VM. free previous user mapping & pages. if any */
    regs->pc = elf.e_entry;  // initial program counter = main
    regs->sp = sp; // initial stack pointer
    trace("pid %d (%s) commit to user VM, sp 0x%lx", myproc()->pid, myproc()->name, sp);

    acquire(&p->mm->lock);
    free_task_pages(p->mm, 1 /*useronly*/);

    // Careful: transfer refcnt/lock from the existing mm
    tmpmm->ref = p->mm->ref; // mm::lock ensures memory barriers needed for mm::ref
    tmpmm->lock = p->mm->lock;
    *(p->mm) = *tmpmm;  // commit (NB: compiled as memcpy())
    p->mm->sz = p->mm->codesz = sz;
    trace("pid: %d, p->mm: %lx, p->mm->sz: 0x%lx", p->pid,(unsigned long)p->mm, p->mm->sz);
    kfree(tmpmm);
    set_pgd(p->mm->pgd);
    release(&p->mm->lock);

    hexdump((char *)sp, USER_TOP - sp, "stack");

    debug("path: %s, pid %d exec succeeds, return %d", path, myproc()->pid, argc);
    return argc; // this ends up in x0, the first argument to main(argc, argv)

bad:
    // release(&p->mm->lock);

    if (tmpmm && tmpmm->pgd) { // new pgtable tree ever allocated ..
        free_task_pages(tmpmm, 1 /*useronly*/);
    }
    if (tmpmm)
        kfree(tmpmm);
    if (ip){
        iunlockput(ip);
    }
    info("exec failed");
    return -1;
}

// Load a program segment into pagetable at virtual address va.
// mm: user mm
// va: user VA, must be page-aligned
// offset: from the beginning of the elf "file"
// sz: segment size, in bytes
// Returns 0 on success, -1 on failure.
//
// Caller must hold mm->lock
static int loadseg(struct mm_struct* mm, uint64_t va, struct inode *ip,
    uint32_t offset, uint32_t sz) {
    uint32_t i, n;
    uint64_t pa;

    uint64_t addr = va & 0x0fffUL;

    debug("va: 0x%lx, ino: %d, offset: 0x%x, sz: 0x%x, addr: 0x%lx", va, ip->inum, offset, sz, addr);
    assert(mm);

    // Verify mapping page by page, then load from file
    for (i = 0; i < sz; i += PAGE_SIZE) {
        pa = walkaddr(mm, va);
        if (pa == 0)
            panic("loadseg: address should exist");
        if (addr > 0) {
            n = (PAGE_SIZE - addr) > sz ? sz : PAGE_SIZE - addr;
            pa += addr;
            sz = sz + PAGE_SIZE - n;
            addr = 0;
        } else if (sz - i < PAGE_SIZE) {
            n = sz - i;
        } else {
            n = PAGE_SIZE;
        }
        // NB: fs func called by readi() may grab sched_lock()
        trace("pa: 0x%lx, offset: 0x%x, n: 0x%x", pa, offset, n);
        if (readi(ip, 0/*to kernel va*/, (uint64_t)P2V(pa), offset, n) != n)
            return -1;
        //hexdump(P2V(pa), n, "loadseq");
        offset += n;
        va += n;
    }

    return 0;
}

