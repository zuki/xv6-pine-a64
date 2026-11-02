#include <types.h>
#include <trap.h>
#include <arm.h>
#include <sysregs.h>
#include <mmu.h>
#include <irq.h>
#include <uart.h>
#include <console.h>
#include <clock.h>
#include <timer.h>
#include <syscall.h>
#include <linux/mman.h>
#include <mmap.h>
#include <vm.h>

static long pf_handler(int ec, int dfs, uint64_t far);

void trap_init(void)
{
    //extern char vectors[];
    //load_vbar_el1(vectors);
    lesr(0);
    info("trap_init ok");
}

void trap(struct trapframe *tf)
{
    //debug("trap");
    uint64_t esr = resr();
    uint64_t far = rfar();
    uint64_t elr = relr();
    int ec  = (int)(esr >> EC_SHIFT);
    int iss = (int)(esr & ISS_MASK);
    int il  = (int)(esr & IL_MASK);
    int dfs = (int)(iss & 0x3f);

   /* Clear esr. */
    lesr(0);

    switch (ec) {
    case EC_UNKNOWN:
        if (il) {
            panic("EC_UNKONW: IL bit on");
        } else {
            irq_handler(/*user_mode(tf)*/);
        }
        //check_pending_signal();
        break;

    case EC_SVC64:
        if (iss == 0) {
            enable_irq();
            tf->x[0] = syscall1(tf);
            trace("ret: 0x%lx", tf->x[0]);
            //check_pending_signal();
            disable_irq();
        } else {
            warn("unexpected svc iss 0x%x", iss);
        }
        break;

    case EC_IABORT:     // 0x20 = 32: ユーザモードで発生した命令例外
    case EC_IABORT2:    // 0x21 = 33: カーネルモードで発生した命令例外
        //info("iabort: esr=0x%llx, ec=%d, dfs=%d, far=0x%llx", esr, ec, dfs, far);
        //exit(1);
        // through
    case EC_DABORT:     // 0x24 = 36: ユーザモードで発生したデータ例外
    case EC_DABORT2:    // 0x25 = 37: カーネルモードで発生したデータ例外
        // 4-7: translation fault at level 0-3
        // 8-11: access flag fault at level 0-3
        // 12-15: permission fault at level 0-3

        if (dfs >= 4 && dfs <= 15) {
            if ((tf->x[0] = pf_handler(ec, dfs, far)) < 0) {
                thisproc()->killed = 1;
                info("inst/dataabort: dfs=%d, far=0x%llx", dfs, far);
                exit(1);
            }
            //check_pending_signal();
        } else {
            info("unknown ec: %d, dfs: %d", ec, dfs);
            trap_error(4);
        }
        //debug("triger ec: %d at elr: 0x%llx with far: 0x%llx", ec, elr, far);
        //exit(1);
        break;
    case EC_ALIGNMENT:  // 0x22 = 34
        error("SP alignment fault exception at elr: 0x%llx with far: 0x%llx", elr, far);
        panic("abort");
        exit(1);
        break;
    default:
        debug("unknown trap code: %d at elr: 0x%llx with far: 0x%llx", ec, elr, far);
        //dump_tf(tf);
        //exit(1);
        panic("unknown");
    }
}

void trap_error(uint64_t type)
{
    //debug_reg();
    error("irq of type %d unimplemented. \n", type);
    panic("trap_error");
}

static long pf_handler(int ec, int dfs, uint64_t far)
{
    struct proc *p = thisproc();
    uint64_t *pte;
    struct vma *vma;
    static int count = 0;

    far = ROUNDDOWN(far, PGSIZE);

    if (dfs <= 7) {             // Translation fault
        // 現在のところロジック上ありえない。何らかのバグなのでエラーとする
        set_pgd((uint64_t)p->pgdir);
        if ((pte = walk(p->pgdir, (void *)far, 1)) == 0) {
            if (count++ < 2)
                error("[%d] recoveary failed: ec=0x%x, dfs=%d, far=0x%llx", p->pid, ec, dfs, far);
            return -1;
        } else {
            if (count++ < 4)
                trace("[%d] return 0: ec=0x%x, dfs=%d, far=0x%llx, pte=0x%llx, *pte=0x%llx", p->pid, ec, dfs, far, pte, *pte);
            return 0;
        }
    } else if (dfs <= 11) {     // Access fault: 遅延読み込み
        vma = p->vmas;
        while (vma) {
            if (vma->addr == (void *)far) {
                if (mmap_load_pages(vma->addr, vma->length, vma->prot, vma->flags, vma->f, vma->offset) < 0) {
                    error("load_pages failed: ec=0x%x, dfs=%d, far=0x%llx, vma=0x%p", ec, dfs, far, vma->addr);
                    return -1;
                }
                set_pgd((uint64_t)p->pgdir);
                return 0;
            }
            vma = vma->next;
        }
        return -1;
    } else {                    // Permission fault: Copy on Write
        vma = p->vmas;
        while (vma) {
            if ((uint64_t)vma->addr <= far
             && far < (uint64_t)vma->addr + vma->length
             && vma->flags & MAP_PRIVATE) {
                if (vma->prot & PROT_WRITE) {
                    uint64_t perm = get_perm(vma->prot, vma->flags);
                    perm &= ~PTE_RO;
                    if (copy_vma_pages(vma->addr, vma->length, perm) < 0) {
                        error("copy_vma_pages failed: ec=0x%x, dfs=%d, far=0x%llx, vma=0x%p, perm=0x%llx", ec, dfs, far, vma->addr, perm);
                        return -1;
                    }
                    set_pgd((uint64_t)p->pgdir);
                    return 0;
                }
            }
            vma = vma->next;
        }
        return -1;
    }
}

void dump_tf(struct trapframe *tf)
{
    cprintf("=== dump trapframe        ===\n");
    cprintf("  spsr: 0x%llx\n", tf->spsr);
    cprintf("   elr: 0x%llx\n", tf->elr);
    cprintf("    sp: 0x%llx\n", tf->sp);
    cprintf(" tpidr: 0x%llx\n", tf->tpidr);
    cprintf("    x1: 0x%llx\n", tf->x[1]);
    cprintf("    x2: 0x%llx\n", tf->x[2]);
    cprintf("    x3: 0x%llx\n", tf->x[3]);
    cprintf("    x8: 0x%llx\n", tf->x[8]);
    cprintf("   x29: 0x%llx\n", tf->x[29]);
    cprintf("   x30: 0x%llx\n", tf->x[30]);
    cprintf("===-----------------------===\n");
}

