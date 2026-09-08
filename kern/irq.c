#include <types.h>
#include <arm.h>
#include <irq.h>
#include <console.h>

static void (*handler[IRQ_LINES])(void *);
static void *params[IRQ_LINES];

void irq_init(void)
{
    for (int i = 0; i < IRQ_LINES; i++) {
        handler[i] = 0;
        params[i] = 0;
    }

    /* GIC情報を取得 */
    uint32_t type = get32(GICD_TYPER);
    debug("GICD_TYPER: [15:11] %x, [10] %x, [7:5] %x, [4:0] %x", (type >> 11) & 0x1f, (type >> 10) & 0x1, (type >> 5) & 0x7, type & 0x1f);
    uint32_t iid = get32(GICD_IIDR);
    debug("GICD_IIDR: Product %x, Variant %x, Rev %x, impl %x", (iid >> 24) & 0xff, (iid >> 16) & 0xf, (iid >> 12) & 0xf, iid & 0xfff);

    /* gicを無効にする */
    put32(GICD_CTLR, GICD_CTLR_DISABLE);

    /* すべての割り込みを無効にし、ペンディグをクリアし、割り込みをディアクティベート */
    for (int n = 0; n < IRQ_LINES / 32; n++) {
        put32(GICD_ICENABLER0 + 4 * n, ~0);
        put32(GICD_ICPENDR0 + 4 * n, ~0);
        put32(GICD_ICACTIVER0 + 4 * n, ~0);
    }

    /* すべての割り込みをデフォルト優先度 (0xa0) でコア0に配信 */
    for (int n = 0; n < IRQ_LINES / 4; n++) {
        put32(GICD_IPRIORITYR0 + 4 * n, GICD_IPRIORITYR_DEFAULT
              | GICD_IPRIORITYR_DEFAULT << 8
              | GICD_IPRIORITYR_DEFAULT << 16
              | GICD_IPRIORITYR_DEFAULT << 24);

        put32(GICD_ITARGETSR0 + 4 * n, GICD_ITARGETSR_CORE0
              | GICD_ITARGETSR_CORE0 << 8
              | GICD_ITARGETSR_CORE0 << 16 | GICD_ITARGETSR_CORE0 << 24);
    }

    /* すべての割り込みをレベルトリガにセット */
    for (int n = 0; n < IRQ_LINES / 16; n++) {
        put32(GICD_ICFGR0 + 4 * n, GICD_ICFGR_LEVEL_SENSITIVE);
    }

    /* gicを有効にする */
    put32(GICD_CTLR, GICD_CTLR_ENABLE);

    /* コア0のCPUインタフェースを初期化 */
    put32(GICC_PMR, GICC_PMR_PRIORITY);
    put32(GICC_CTLR, GICC_CTLR_ENABLE);

    info("irq_init ok");
}

void irq_enable(int i)
{
    put32(GICD_ISENABLER0 + 4 * (i / 32), 1 << (i % 32));
}


void irq_disable(int i)
{
    put32(GICD_ICENABLER0 + 4 * (i / 32), 1 << (i % 32));
}

/*
 * 割り込みハンドラを登録する.
 * これにより割り込みiが発生した場合、関数fが呼び出される
 */
void irq_register(int i, void (*f)(void *), void *param)
{
    handler[i] = f;
    params[i] = param;
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
            nack += handle1(i);
        } else {
            /* Software generated interrupts (SGI). */
            uint32_t core =
                (iar & GICC_IAR_CPUID__MASK) >> GICC_IAR_CPUID__SHIFT;

            panic(__FILE__, __LINE__, "unimplemented, irq %d, core %d", i, core);
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
    panic(__FILE__, __LINE__, "irq_error");
}
