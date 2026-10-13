#include <types.h>
#include <sched.h>
#include <mmu.h>
#include <elf.h>
#include <file.h>
#include <console.h>

// fxl: derived from xv6, for which user stack NOT at the very end of va
//  but next page boundary of user code (with 1 pg of stack guard
//  in between)   this limits to user stack to 4KB. (not growing)
//  (why designed like this??)
static int loadseg(struct mm_struct *mm, uint64_t va, struct inode *ip,
  uint32_t offset, uint32_t sz);

static int flags2perm(int flags)
{
    // elf, https://refspecs.linuxbase.org/elf/gabi4+/ch5.pheader.html#p_flags
#define   PF_X    1
#define   PF_W    2
#define   PF_R    4

    int perm = 0;
    assert(flags & PF_R);

    if(flags & PF_W)
      perm = MM_AP_RW;
    else
      perm = MM_AP_EL0_RO;

    if(!(flags & PF_X))
      perm |= MM_XN;

    return perm;
}


// Called from sys_exec
// for now, no support for exec() on fat fs
int exec(char *path, char **argv, char **envp)
{
    char *s, *last;
    int i, off;
    uint64_t argc, sz = 0, sz1, sp, ustack[MAXARG], argbase;
    struct mm_struct *tmpmm = 0;  // new mm to prep aside, and commit to user VM
    Elf64_Ehdr elf;
    struct inode *ip;
    Elf64_Phdr ph;
    void *kva;
    struct task_struct *p = myproc();

    trace("pid %d exec called. path %s", myproc()->pid, path);

    if((ip = namei(path, AT_FDCWD)) == 0){
        return -1;
    }
    ilock(ip);

    // no need to lock tmpmm->lock -- tmpmm is private
    // acquire(&p->mm->lock);

    if (readi(ip, 0, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf)) // ELF header good?
        goto bad;

    if(elf.magic != ELF_MAGIC) // ELF magic good?
        goto bad;

    if(p->mm->pgd == 0) // task has a valid pgtable tree?
        goto bad;   // XXXX race condition; need lock

    // for simplicity, we alloc a a single page for mm_struct
    _Static_assert(sizeof(struct mm_struct) <= PAGE_SIZE);
    tmpmm = kalloc(1);
    assert(tmpmm);

    initlock(&tmpmm->lock, "tmpmmlock");

    tmpmm->kernel_pages_count = p->mm->kernel_pages_count;
    memmove(&tmpmm->kernel_pages, &p->mm->kernel_pages, sizeof(tmpmm->kernel_pages));
    tmpmm->pgd = 0; // start from a fresh pgtable tree...

    // Load program into memory of tmpmm: iterate over all elf sections
    // NB: code below assumes section vaddrs in ascending order
    for(i=0, off=elf.e_phoff; i<elf.e_phnum; i++, off+=sizeof(ph)){
        // loader a section header
        if (readi(ip, 0, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;
        if (ph.p_type != PT_LOAD)
            continue;
        trace("pid %d vaddr %lx sz %lx", myproc()->pid, ph.p_vaddr, sz);
        assert(ph.p_vaddr >= sz); // sz: last seg's va end (exclusive)
        if (ph.p_memsz < ph.p_filesz)  // memsz: seg size in mem; filesz: seg bytes to load from file
            goto bad;
        // a section's start must be page aligned, but end does not have to
        if (ph.p_vaddr + ph.p_memsz < ph.p_vaddr)
            goto bad;
        if (ph.p_vaddr % PAGE_SIZE != 0)
            goto bad;
        for (sz1 = ph.p_vaddr; sz1 < ph.p_vaddr + ph.p_memsz; sz1 += PAGE_SIZE) {
            kva = allocate_user_page_mm(tmpmm, sz1, flags2perm(ph.p_flags));
            assert(kva);
        }
        sz = sz1;
        if (loadseg(tmpmm, ph.p_vaddr, ip, ph.p_offset, ph.p_filesz) < 0)
            goto bad;
    }
    iunlockput(ip);
    ip = 0;

    trace("pid %d done loading prog", myproc()->pid);

    sz = PGROUNDUP(sz);

    /* Alloc a fresh user stack  */
    assert(sz + PAGE_SIZE + USER_MAX_STACK <= USER_VA_END);
    // alloc the 1st stack page (instead of demand paging), for pushing args (below)
    if (!(kva=allocate_user_page_mm(tmpmm, USER_VA_END - PAGE_SIZE, MMU_PTE_FLAGS | MM_AP_RW))) {
        goto bad;
    }
    memzero_aligned(kva, PAGE_SIZE);
    // map a guard page (inaccessible) near USER_MAX_STACK
    if (!(kva=allocate_user_page_mm(tmpmm, USER_VA_END - USER_MAX_STACK - PAGE_SIZE, MMU_PTE_FLAGS | MM_AP_EL1_RW))) {
        goto bad;
    }

    /* Prep prog arguments on user stack  */
    sp = USER_VA_END;
    argbase = USER_VA_END - PAGE_SIZE; // args at most 1 PAGE
    // Push argument strings, prepare rest of stack in ustack.
    // Populate arg strings on stack. save arg pointers to ustack (a scratch buf)
    //    then populate "ustack" on the stack
    for (argc = 0; argv[argc]; argc++) {
        if (argc >= MAXARG)
            goto bad;
        sp -= strlen(argv[argc]) + 1;
        sp -= sp % 16; // riscv sp must be 16-byte aligned  fxl: do we need this?
        if (sp < argbase)
            goto bad;
        if (copyout(tmpmm, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
            goto bad;
        ustack[argc] = sp;
    }
    ustack[argc] = 0; // last arg

    // push the array of argv[] pointers.
    sp -= (argc+1) * sizeof(uint64_t);
    sp -= sp % 16;  // fxl: do we need this
    if (sp < argbase)
        goto bad;
    if (copyout(tmpmm, sp, (char *)ustack, (argc+1)*sizeof(uint64_t)) < 0)
        goto bad;

    // arguments to user main(argc, argv)
    // argc is returned via the system call return value, which goes in r0.
    struct trapframe *regs = task_pt_regs(p);
    regs->regs[1] = sp;

    // Save program name for debugging.
    for(last=s=path; *s; s++)
        if(*s == '/')
        last = s+1;
    safestrcpy(p->name, last, sizeof(p->name));

    /* Commit to the user VM. free previous user mapping & pages. if any */
    regs->pc = elf.e_entry;  // initial program counter = main
    regs->sp = sp; // initial stack pointer
    V("pid %d (%s) commit to user VM, sp 0x%lx", myproc()->pid, myproc()->name, sp);

    acquire(&p->mm->lock);
    free_task_pages(p->mm, 1 /*useronly*/);

    // Careful: transfer refcnt/lock from the existing mm
    tmpmm->ref = p->mm->ref; // mm::lock ensures memory barriers needed for mm::ref
    tmpmm->lock = p->mm->lock;
    *(p->mm) = *tmpmm;  // commit (NB: compiled as memcpy())
    p->mm->sz = p->mm->codesz = sz;
    V("pid %d p->mm %lx p->mm->sz %lu", p->pid,(unsigned long)p->mm, p->mm->sz);
    kfree(tmpmm);
    set_pgd(p->mm->pgd);
    release(&p->mm->lock);

    V("pid %d exec succeeds", myproc()->pid);
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
        end_op();
    }
    I("exec failed");
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

  assert(va % PAGE_SIZE == 0);
  assert(mm);

  // Verify mapping page by page, then load from file
  for(i = 0; i < sz; i += PAGE_SIZE){
    pa = walkaddr(mm, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PAGE_SIZE)
      n = sz - i;
    else
      n = PAGE_SIZE;
    // NB: fs func called by readi() may grab sched_lock()
    if(readi(ip, 0/*to kernel va*/, (uint64_t)PA2VA(pa), offset+i, n) != n)
      return -1;
  }

  return 0;
}
