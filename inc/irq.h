/* See BCM2837 ARM Peripherals and QA7_rev3.4. */
#ifndef INC_PERIPHERALS_IRQ_H
#define INC_PERIPHERALS_IRQ_H

#include <peripherals/base.h>

#define GICD_BASE               (MMIO_BASE + 0x01c81000)
#define GICC_BASE               (MMIO_BASE + 0x01c82000)

/* GIC distributor registers. */
#define GICD_CTLR               (GICD_BASE + 0x000)     // Distributor Control Register
/* non-secure access */
#define     GICD_CTLR_DISABLE       (0 << 0)
#define     GICD_CTLR_ENABLE        (1 << 0)

/* Secure access. */
#define     GICD_CTLR_ENABLE_GROUP0 (1 << 0)
#define     GICD_CTLR_ENABLE_GROUP1 (1 << 1)

#define GICD_TYPER              (GICD_BASE + 0x004)     // Interrupt Controller Type Register
#define GICD_IIDR               (GICD_BASE + 0x008)     // Distributor Implementer Indentification
#define GICD_IGROUPRn           (GICD_BASE + 0x080)     // Interrupt Group Register
#define GICD_ISENABLERn         (GICD_BASE + 0x100)     // Interrupt Set-Enable Register
#define GICD_ICENABLERn         (GICD_BASE + 0x180)     // Interrupt Clear-Enable Register
#define GICD_ISPENDRn           (GICD_BASE + 0x200)     // Interrupt Set-Pending Register
#define GICD_ICPENDRn           (GICD_BASE + 0x280)     // Interrupt Clear-Pending Register
#define GICD_ISACTIVERn         (GICD_BASE + 0x300)     // Interrupt Set-Active Register
#define GICD_ICACTIVERn         (GICD_BASE + 0x380)     // Interrupt Clear-Active Register
#define GICD_IPRIORITYRn        (GICD_BASE + 0x400)     // Interrupt Priority Register
#define     GICD_IPRIORITYR_DEFAULT 0xA0
#define     GICD_IPRIORITYR_FIQ     0x40

#define GICD_ITARGETSRn         (GICD_BASE + 0x800)     // Interrupt Processor Target Register
#define     GICD_ITARGETSR_CORE0    (1 << 0)

#define GICD_ICFGRn             (GICD_BASE + 0xC00)     // Interrupt Configuration Register
#define     GICD_ICFGR_LEVEL_SENSITIVE  (0 << 1)
#define     GICD_ICFGR_EDGE_TRIGGERED   (1 << 1)

#define GICD_IGROUPMODRn        (GICD_BASE + 0x0d00)
#define GICD_NSACRn             (GICD_BASE + 0x0e00)    // Non-secure Access Control Registers, optional

#define GICD_SGIR               (GICD_BASE + 0xF00)     // Software Generated Interrupt Register
#define     GICD_SGIR_SGIINTID__MASK            0x0F
#define     GICD_SGIR_CPU_TARGET_LIST__SHIFT    16
#define     GICD_SGIR_TARGET_LIST_FILTER__SHIFT 24

#define GICD_PIDR2_GICV2        (GICD_BASE + 0xFE8)
#define     GICD_PIDR2__MASK            0x0F
#define     GICD_PIDR2__SHIFT           4

/* GIC CPU interface registers. */
#define GICC_CTLR               (GICC_BASE + 0x000)     // CPU Iinterface Control Register
#define     GICC_CTLR_EOIMODENS     (1 << 9)
#define     GICC_CTLR_IRQBYPDISGRP1  (1 << 6)
#define     GICC_CTLR_FIRQBYPDISGRP1 (1 << 5)
#define     GICC_CTLR_DISABLE       (0 << 0)
#define     GICC_CTLR_ENABLE        (1 << 0)

/* Secure access. */
#define     GICC_CTLR_ENABLE_GROUP0 (1 << 0)
#define     GICC_CTLR_ENABLE_GROUP1 (1 << 1)

#define GICC_PMR                (GICC_BASE + 0x004)     // Interrupt Priority Mask Register
#define     GICC_PMR_PRIORITY       (0xFF << 0)

#define GICC_BPR                (GICC_BASE + 0x0008)    // Binary point Register
#define     GICC_BRP_NOPREMPT       (7 << 0)

#define GICC_IAR                (GICC_BASE + 0x00C)     // Interrupt Acknowledge Register
#define GICC_IAR_INTERRUPT_ID__MASK 0x3FF
#define GICC_IAR_CPUID__SHIFT       10
#define GICC_IAR_CPUID__MASK        (3 << 10)

#define GICC_EOIR               (GICC_BASE + 0x010)     // End of Interrupt Register
#define     GICC_EOIR_EOIINTID__MASK    0x3FF
#define     GICC_EOIR_CPUID__SHIFT      10
#define     GICC_EOIR_CPUID__MASK       (3 << 10)

/* 割り込み番号 */
#define GIC_IRQ_SGI         0
#define GIC_IRQ_PPI         16
#define GIC_IRQ_SPI         32

#define GIC_SGI(n)          (GIC_IRQ_SGI + (n))     // ソフトウェア生成割り込み
#define GIC_PPI(n)          (GIC_IRQ_PPI + (n))     // コアプライベート割り込み
#define GIC_SPI(n)          (GIC_IRQ_SPI + (n))     // コア共有割り込み

#define IRQ_UART0           GIC_SPI(0)      // 32
#define IRQ_TIMER0          GIC_SPI(0x12)   // 50
#define IRQ_TIMER1          GIC_SPI(0x13)   // 51
#define IRQ_MSGBOX          GIC_SPI(0x31)   // 81
#define IRQ_DMA             GIC_SPI(0x32)   // 82
#define IRQ_HSTIMER         GIC_SPI(0x33)   // 83
#define IRQ_SD0             GIC_SPI(0x3c)   // 92
#define IRQ_SD1             GIC_SPI(0x3d)   // 93
#define IRQ_OTG_EHCI        GIC_SPI(0x48)   // 104
#define IRQ_OTG_OHCI        GIC_SPI(0x49)   // 105
#define IRQ_EHCI0           GIC_SPI(0x4a)   // 106
#define IRQ_OHCI0           GIC_SPI(0x4b)   // 107
#define IRQ_EMAC            GIC_SPI(0x52)   // 114

#define IRQ_LINES           224

#define CCU_BASE                (MMIO_BASE + 0x01c20000)
#define PLL_PERIPH0_CTRL_REG    (CCU_BASE + 0x0028)
#define     PLL_PERIPH0_CTRL_ENABLE     (1 << 31)
#define     PLL_PERIPH0_CTRL_LOCK       (1 << 28)
#define     PLL_PERIPH0_CTRL_24M_OUT_EN (1 << 18)
#define     PLL_PERIPH0_CTRL_FACTOR_N   (0x18 << 8)
#define     PLL_PERIPH0_CTRL_FACTOR_K   (1 << 4)
#define     PLL_PERIPH0_CTRL_FACTOR_M   (1 << 0)
#define AHB1_APB1_CFG_REG       (CCU_BASE + 0x0054)
#define     AHB1_CFG_AHB1_CLK_SRC_SEL   (3 << 12)
#define     AHB1_CFG_APB1_CLK_RATIO     (1 << 8)
#define     AHB1_CFG_AHB1_PRE_DIV       (2 << 6)
#define     AHB1_CFG_AHB1_CLK_DIV_RATIO (1 << 4)
#define BUS_CLK_GATING_REG0     (CCU_BASE + 0x0060)
#define     BUS_CLK_GATING_REG0_HSTMR_GATING    (1 << 19)
#define BUS_SOFT_RST_REG0       (CCU_BASE + 0x02c0)
#define     BUS_SOFT_RST0_HSTMR_RST     (1 << 19)

#define BUS_CLK_GATING_REG3     (CCU_BASE + 0x006c)
#define     BUS_CLK_GATING_REG0_UART0_GATING    (1 << 16)
#define BUS_SOFT_RST_REG4       (CCU_BASE + 0x02d8)
#define     BUS_SOFT_RST0_UART0_RST     (1 << 16)

void irq_init(void);
void irq_enable(int i);
void irq_disable(int i);
void irq_register(int i, void (*f)(void *), void *param);
void irq_handler(void/* int user_mode */);
void irq_error(uint64_t type);
void irq_send_sgi_to_me(uint32_t irqno);

#endif
