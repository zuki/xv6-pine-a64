#include <types.h>
#include <arm.h>
#include <irq.h>
#include <console.h>
#include <vectors.h>

// must match entry.h
const char *entry_error_messages[] = {
    [SYNC_INVALID_EL1t]     = "SYNC_INVALID_EL1t",
    [IRQ_INVALID_EL1t]      = "IRQ_INVALID_EL1t",
    [FIQ_INVALID_EL1t]      = "FIQ_INVALID_EL1t",
    [ERROR_INVALID_EL1t]    = "ERROR_INVALID_EL1t",

    [SYNC_INVALID_EL1h]     = "SYNC_INVALID_EL1h",
    [FIQ_INVALID_EL1h]      = "FIQ_INVALID_EL1h",
    [ERROR_INVALID_EL1h]    = "ERROR_INVALID_EL1h",

    [FIQ_INVALID_EL0_64]    = "FIQ_INVALID_EL0_64",
    [ERROR_INVALID_EL0_64]  = "ERROR_INVALID_EL0_64",

    [SYNC_INVALID_EL0_32]   = "SYNC_INVALID_EL0_32",
    [IRQ_INVALID_EL0_32]    = "IRQ_INVALID_EL0_32",
    [FIQ_INVALID_EL0_32]    = "FIQ_INVALID_EL0_32",
    [ERROR_INVALID_EL0_32]  = "ERROR_INVALID_EL0_32",

    [SYNC_ERROR]            = "Unhandled EL0 sync exception",
    [SYSCALL_ERROR]         = "SYSCALL_ERROR",
    [DATA_ABORT_ERROR]      = "Unhandled EL0 data abort (after kernel's trying)",
    [UNDEFUN_SYSCALL]       = "Undefined syscall"
};

static void (*handler[IRQ_LINES])(void *);
static void *params[IRQ_LINES];

static uint32_t arm_gic_nlines(void)
{
    return 32 * ((get32(GICD_TYPER) & 0x1f) + 1);
}

void irq_init_gicd(void)
{
    for (int i = 0; i < IRQ_LINES; i++) {
        handler[i] = 0;
        params[i] = 0;
    }

    put32(BUS_CLK_GATING_REG0, get32(BUS_CLK_GATING_REG0) | BUS_CLK_GATING_REG0_HSTMR_GATING);
    put32(BUS_SOFT_RST_REG0, get32(BUS_SOFT_RST_REG0) | BUS_SOFT_RST0_HSTMR_RST);

    //debug("cntfrq_el0: 0x%x", get_cntfrq_el0());  // 24MHz
#if 0
    debug("SPC3: 0x%x", get32(0x01c23428));     // 0x0
    debug("SPC4: 0x%x", get32(0x01c23434));     // 0x0
    put32(0x01c23434, 0x1);
    isb(); dsb();
    debug("SPC4: 0x%x", get32(0x01c23434));     // 0x0

    /* GIC情報を取得 */
    uint32_t type_value = get32(GICD_TYPER);    // 0xfc66 : 11111_1_00_011_00110
    //int IRQ_LINES = 32 * ((type_value & 0x1f) + 1);   // 224
    debug("GICD_TYPER: 0x%x", type_value);
    debug("GICD_IIDR:  0x%x", get32(GICD_IIDR));    // 0x200143b
    debug("GICD_CTLR:  0x%x", get32(GICD_CTLR));    // 0x0
    uint32_t gicv = get32(GICD_PIDR2_GICV2);
    debug("GICD_PIDR2_GICV2: 0x%x, v=%d", gicv, (gicv >> GICD_PIDR2__SHIFT) & GICD_PIDR2__MASK);

    for (int n = 0; n < IRQ_LINES / 32; n++) {
        // GICD_IGROUPRはノンセキュアアクセスでは RAZ/WI (Read As Zero, Write Ignored)
        //debug("GICD_IGROUPR[%d]:    0x%x", n, get32(GICD_IGROUPRn + 4 * n));
        debug("GICD_ISENABLER[%d]:  0x%x", n, get32(GICD_ISENABLERn + 4 * n));
    }
#endif

    uint32_t nlines = arm_gic_nlines();
    uint32_t idx;

    /* gicdを無効にする */
    put32(GICD_CTLR, GICD_CTLR_DISABLE);

    // SGIとPPIの設定
    put32(GICD_ICENABLERn, 0xffffffff);
    put32(GICD_ICPENDRn, 0xffffffff);
    put32(GICD_ICACTIVERn, 0xffffffff);

    for (uint32_t i = 0; i < 2; i++) {
        put32(GICD_ICFGRn + 4 * i, 0x55555555);
    }

    // GICD_ITARGETSR 0-7はRead Only
    for (uint32_t i = 0; i < 8 ; i++) {
        put32(GICD_IPRIORITYRn + 4 * i, 0x80808080);    // SGIとPPIの優先度
    }

    // SPIの設定
    // nlines = 224
    // 1レジスタ 32 irq -> 7 レジスタ
    // すべての割り込みを無効にし、ペンディグをクリアし、割り込みをディアクティベート
    for (idx = (GIC_IRQ_SPI / 32); idx < (nlines / 32); idx++) {
        put32(GICD_ICENABLERn + 4 * idx, 0xffffffff);
        put32(GICD_ICPENDRn + 4 * idx, 0xffffffff);
        put32(GICD_ICACTIVERn + 4 * idx, 0xffffffff);
    }

    //　1レジスタ 16 irq -> 14 レジスタ
    // すべてのSPI割り込みをレベルトリガにセット
    // 　　1割り込み2ビットで、高位ビットが0はレベルトリガ、1はエッジトリガ
    for (idx = (GIC_IRQ_SPI / 16); idx < (nlines / 16); idx++) {
        put32(GICD_ICFGRn + 4 * idx, 0x55555555);       /* SPIs level sensitive */
    }

    // 1レジスタ 4 irq -> 56 レジスタ
    // すべてのSPI割り込みをデフォルト優先度 (0xa0) で、すべてのSPI割り込みはコアに配信しない（登録時に個別に設定）
    //    1割り込み8bit、優先度は値が小さいほど優先度が高い
    for (idx = (GIC_IRQ_SPI / 4); idx < (nlines / 4); idx++) {
        put32(GICD_IPRIORITYRn + 4 * idx, 0xa0a0a0a0);  /* SPI priority */
        put32(GICD_ITARGETSRn + 4 * idx, 0x00000000);   /* SPI to no cpu */
    }

    // gicdを有効にする
    put32(GICD_CTLR, GICD_CTLR_ENABLE);

    info("cpu0_init ok");
}

void irq_init_gicc(int core)
{
    uint32_t iccicr;

    // giccを無効にする
    put32(GICC_CTLR, 0x60);             // not signal Bypass IRQ/FIQ, Grp1 irq

    put32(GICC_BPR, GICC_BRP_NOPREMPT);     // プリエンプションをすべて無効にする */
    put32(GICC_PMR, GICC_PMR_PRIORITY);     // 0xFFより高い優先度の割り込みを通知（0xf0は通知しないので注意）

    iccicr = get32(GICC_CTLR);
    // 以下で構成するために非セキュアアクセス時のGICC_CTLRビットをクリア */
    iccicr &= ~(GICC_CTLR_EOIMODENS | GICC_CTLR_IRQBYPDISGRP1 | GICC_CTLR_FIRQBYPDISGRP1 | GICC_CTLR_ENABLE);
    // グループ1を有効化し、グループ1のバイパスを無効化する
    iccicr |= (GICC_CTLR_IRQBYPDISGRP1 | GICC_CTLR_FIRQBYPDISGRP1 | GICC_CTLR_ENABLE);
    put32(GICC_CTLR, iccicr);

    info("cpu%d_init ok", cpuid());
}

void irq_enable(int i)
{
    int idx = i / 32;
    put32(GICD_ISENABLERn + 4 * idx, 1 << (i % 32));

    trace("GICD_ISENABLER[%d]: 0x%08x", idx, get32(GICD_ISENABLERn + 4 * idx));
}


void irq_disable(int i)
{
    put32(GICD_ICENABLERn + 4 * (i / 32), 1 << (i % 32));
}

/*
 * 割り込みハンドラを登録する.
 * これにより割り込みiが発生した場合、関数fが呼び出される
 */
void irq_register(int i, void (*f)(void *), void *param)
{
    handler[i] = f;
    params[i] = param;
    trace("registered: irq %d f %p", i, f);
}

static int handle1(int i)
{
    if (handler[i]) {
        handler[i](params[i]);
        return 1;
    } else {
        debug("no handler for irq %d", i);
    }
    return 0;
}

void irq_handler(void/*int user_mode */)
{
    int nack = 0;
    uint32_t iar = get32(GICC_IAR);
    uint32_t i = iar & GICC_IAR_INTERRUPT_ID__MASK;

    if (i >= 1020) {
        put32(GICC_EOIR, iar);
        return;
    }

    if (i < GIC_IRQ_PPI) {          // SGI
        uint32_t core =
                (iar & GICC_CPUID__MASK) >> GICC_CPUID__SHIFT;
        debug("unimplemented, irq %d, core %d", i, core);
    } else if (i < GIC_IRQ_SPI) {   // PPI
        nack += handle1(i);
    } else if (i >= GIC_IRQ_SPI) {  // SPI
        nack += handle1(i);
    } else {
        panic("unimplemented, i: %d", i);
    }

    put32(GICC_EOIR, iar);

    if (nack == 0) {
        trace("unexpected interrupt, maybe sdhost or EC_UNKNOWN");
    }
}

void irq_error(uint64_t type)
{
    error("irq of type %d unimplemented", type);
    //panic("irq_error");
}

void irq_send_sgi_to_me(uint32_t irqno)
{
    if (irqno < 0 && irqno > 15) {
        error("invalid irqno: %u", irqno);
        return;
    }

    put32(GICD_SGIR, (2 << 24) | (irqno & 0xf));
}

void show_invalid_entry_message(int type, uint64_t esr,
    uint64_t elr, uint64_t far, uint64_t sysno)
{
    if (sysno) {
        error("%s: %ld, esr: 0x%016llx, elr: 0x%016llx, far: 0x%016llx",
        entry_error_messages[type], sysno, esr, elr, far);
    } else {
        error("%s, esr: 0x%016llx, elr: 0x%016llx, far: 0x%016llx",
        entry_error_messages[type], esr, elr, far);
    }
    error("online esr decoder: %s0x%016llx", "https://esr.arm64.dev/#", esr);
    //show_stack(myproc(), "");
}

