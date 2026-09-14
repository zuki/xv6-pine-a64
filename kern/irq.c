#include <types.h>
#include <arm.h>
#include <irq.h>
#include <console.h>

static void (*handler[IRQ_LINES])(void *);
static void *params[IRQ_LINES];

static uint32_t arm_gic_nlines(void)
{
    return 32 * ((get32(GICD_TYPER) & 0x1f) + 1);
}

static void cpu0_init(void)
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
    uint32_t irq;

    /* gicdを無効にする */
    put32(GICD_CTLR, GICD_CTLR_DISABLE);

    for (irq = GIC_IRQ_SPI; irq < nlines / 32; irq += 32) {
        put32(GICD_IGROUPRn + 4 * irq, 0x00000000);     /* SPIs group 0 */
        put32(GICD_ICENABLERn + 4 * irq, 0xffffffff);   /* SPIs disabled */
    }

    for (irq = GIC_IRQ_SPI; irq < nlines / 16; irq += 16) {
        put32(GICD_ICFGRn + 4 + irq, 0x55555555);       /* SPIs level sensitive */
    }

    for (irq = GIC_IRQ_SPI; irq < nlines / 8; irq += 4) {
        put32(GICD_IPRIORITYRn + 4 * irq, 0xf0f0f0f0);  /* SPI priority */
        put32(GICD_ITARGETSRn + 4 * irq, 0x01010101);   /* SPI on CPU0 */
    }

#if 0
    /* gicを無効にする */
    put32(GICD_CTLR, GICD_CTLR_DISABLE);

    /* すべての割り込みを無効にし、ペンディグをクリアし、割り込みをディアクティベート : n < 4 */
    /*   1割り込み1ビット */
    for (int n = 0; n < IRQ_LINES / 32; n++) {
        put32(GICD_ICENABLERn + 4 * n, 0xffffffff);
        put32(GICD_ICPENDRn + 4 * n, 0xffffffff);
        put32(GICD_ICACTIVERn + 4 * n, 0xffffffff);
    }

    /* すべての割り込みをデフォルト優先度 (0xa0) で、すべての割り込みをコア0だけに配信 : n < 39 */
    /*    1割り込み8bit、優先度は値が小さいほど優先度が高い */
    for (int n = 0; n < IRQ_LINES / 4; n++) {
        put32(GICD_IPRIORITYRn + 4 * n, GICD_IPRIORITYR_DEFAULT
              | GICD_IPRIORITYR_DEFAULT << 8
              | GICD_IPRIORITYR_DEFAULT << 16
              | GICD_IPRIORITYR_DEFAULT << 24);
        /* 8ビットのうち 0x1はコア0に、0x2はコア1に、0x4はコア2に、0x8はコア3に配信する */
        /*  ORでつなげることで複数のコアに配信できる */
        put32(GICD_ITARGETSRn + 4 * n, GICD_ITARGETSR_CORE0
              | GICD_ITARGETSR_CORE0 << 8
              | GICD_ITARGETSR_CORE0 << 16 | GICD_ITARGETSR_CORE0 << 24);
    }

    /* すべての割り込みをレベルトリガにセット : n < 9 */
    /* 　　1割り込み2ビットで、高位ビットが0はレベルトリガ、1はエッジトリガ */
    for (int n = 0; n < IRQ_LINES / 16; n++) {
        put32(GICD_ICFGRn + 4 * n, GICD_ICFGR_LEVEL_SENSITIVE);
    }

    /* コア0のCPUインタフェースを初期化 */
    /* 割り込みを通知する優先度のしきい値を0xF0とする */
    put32(GICC_PMR, GICC_PMR_PRIORITY);
#endif
    //info("cpu0_init ok");
}

static void cpun_init(void)
{
    uint32_t iccicr;

    put32(GICD_IGROUPRn, 0x00000000);       /* SGIとPPIはセキュア */
    put32(GICD_ICENABLERn, 0xfe000000);     /* PPIは無効 */

    for (uint32_t i = 0; i < 32; i += 4) {
        put32(GICD_IPRIORITYRn + 4 * i, 0x80808080);    /* SGIとPPIの優先度 */
    }

    put32(GICC_BPR, GICC_BRP_NOPREMPT); /* プリエンプションをすべて無効にする */
    put32(GICC_PMR, GICC_PMR_PRIORITY); /* Program the idle priority in the PMR */

    iccicr = get32(GICC_CTLR);
    /* Clear non-secure state GICC_CTLR bits to be configured below */
    iccicr &= ~(GICC_CTLR_EOIMODENS | GICC_CTLR_IRQBYPDISGRP1 | GICC_CTLR_FIRQBYPDISGRP1 | GICC_CTLR_ENABLE);
    /* Enable the Group 1 interrupts and disable Group 1 bypass. */
    iccicr |= (GICC_CTLR_IRQBYPDISGRP1 | GICC_CTLR_FIRQBYPDISGRP1 | GICC_CTLR_ENABLE);
    put32(GICC_CTLR, iccicr);

    /* gicdを有効にする */
    put32(GICD_CTLR, GICD_CTLR_ENABLE);

    //info("cpu%d_init ok", cpuid());
}

void irq_init(void)
{
    if (cpuid() == 0)
        cpu0_init();

    cpun_init();

    //info("irq_init ok");
}

void irq_enable(int i)
{
    int idx = i / 32;
    put32(GICD_ISENABLERn + 4 * idx, 1 << (i % 32));

    trace("GICD_ISENABLER[%d]: 0x%x", idx, get32(GICD_ISENABLERn + 4 * idx));
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
    if (i < IRQ_LINES) {
        if (i > 15) {
            /* Peripheral interrupts (PPI and SPI). */
            if (i == IRQ_UART0) {
                debug("IRQ_UART0 fired");
            } else if (i == IRQ_UART0) {
                debug("IRQ_UART0 fired");
            }
            nack += handle1(i);
        } else {
            /* Software generated interrupts (SGI). */
            uint32_t core =
                (iar & GICC_IAR_CPUID__MASK) >> GICC_IAR_CPUID__SHIFT;

            panic("unimplemented, irq %d, core %d", i, core);
        }
        put32(GICC_EOIR, iar);
    }

    if (nack == 0) {
        trace("unexpected interrupt, maybe sdhost or EC_UNKNOWN");
    }
}

void irq_error(uint64_t type)
{
    error("irq of type %d unimplemented", type);
    panic("irq_error");
}

void irq_send_sgi_to_me(uint32_t irqno)
{
    if (irqno < 0 && irqno > 15) {
        error("invalid irqno: %u", irqno);
        return;
    }

    put32(GICD_SGIR, (2 << 24) | (irqno & 0xf));
}
