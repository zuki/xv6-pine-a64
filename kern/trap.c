#include <trap.h>

#include <arm.h>
#include <sysregs.h>
#include <mmu.h>
#include <irq.h>

#include <uart.h>
#include <console.h>
#include <clock.h>
#include <timer.h>

void trap_init(void)
{
    extern char vectors[];
    load_vbar_el1(vectors);
    lesr(0);
    //info("trap_init ok");
}

void trap(struct trapframe *tf)
{
    //debug("trap");
    uint64_t esr = resr();
    uint64_t far = rfar();
    uint64_t elr = relr();
    int ec  = (int)(esr >> EC_SHIFT);
    //int iss = (int)(esr & ISS_MASK);
    int il  = (int)(esr & IL_MASK);
    //int dfs = (int)(iss & 0x3f);

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
#if 0
        if (iss == 0) {
            tf->x[0] = syscall1(tf);
            //check_pending_signal();
        } else {
            warn("unexpected svc iss 0x%x", iss);
        }
#endif
        debug("triger syscall");
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
#if 0
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
#endif
        debug("triger ec: %d at elr: 0x%llx with far: 0x%llx", ec, elr, far);
        //exit(1);
        break;
    case EC_ALIGNMENT:  // 0x22 = 34
        error("SP alignment fault exception at elr: 0x%llx with far: 0x%llx", elr, far);
        panic("abort");
        //exit(1);
        break;
    default:
        debug("unknown trap code: %d at elr: 0x%llx with far: 0x%llx", ec, elr, far);
        //dump_tf(tf);
        //exit(1);
        panic("unknown");
    }

}

