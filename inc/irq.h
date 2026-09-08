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
#define GICD_IGROUPR0           (GICD_BASE + 0x080)     // Interrupt Group Register
#define GICD_ISENABLER0         (GICD_BASE + 0x100)     // Interrupt Set-Enable Register
#define GICD_ICENABLER0         (GICD_BASE + 0x180)     // Interrupt Clear-Enable Register
#define GICD_ISPENDR0           (GICD_BASE + 0x200)     // Interrupt Set-Pending Register
#define GICD_ICPENDR0           (GICD_BASE + 0x280)     // Interrupt Clear-Pending Register
#define GICD_ISACTIVER0         (GICD_BASE + 0x300)     // Interrupt Set-Active Register
#define GICD_ICACTIVER0         (GICD_BASE + 0x380)     // Interrupt Clear-Active Register
#define GICD_IPRIORITYR0        (GICD_BASE + 0x400)     // Interrupt Priority Register
#define     GICD_IPRIORITYR_DEFAULT 0xA0
#define     GICD_IPRIORITYR_FIQ     0x40

#define GICD_ITARGETSR0         (GICD_BASE + 0x800)     // Interrupt Processor Target Register
#define     GICD_ITARGETSR_CORE0    (1 << 0)

#define GICD_ICFGR0                 (GICD_BASE + 0xC00) // Interrupt Configuration Register
#define     GICD_ICFGR_LEVEL_SENSITIVE  (0 << 1)
#define     GICD_ICFGR_EDGE_TRIGGERED   (1 << 1)

#define GICD_SGIR                           (GICD_BASE + 0xF00) // Software Generated Interrupt Register
#define     GICD_SGIR_SGIINTID__MASK            0x0F
#define     GICD_SGIR_CPU_TARGET_LIST__SHIFT    16
#define     GICD_SGIR_TARGET_LIST_FILTER__SHIFT 24

/* GIC CPU interface registers. */
#define GICC_CTLR               (GICC_BASE + 0x000)
#define GICC_CTLR_DISABLE       (0 << 0)
#define GICC_CTLR_ENABLE        (1 << 0)
/* Secure access. */
#define GICC_CTLR_ENABLE_GROUP0 (1 << 0)
#define GICC_CTLR_ENABLE_GROUP1 (1 << 1)
#define GICC_CTLR_FIQ_ENABLE    (1 << 3)

#define GICC_PMR                (GICC_BASE + 0x004)
#define GICC_PMR_PRIORITY       (0xF0 << 0)

#define GICC_IAR                    (GICC_BASE + 0x00C)
#define GICC_IAR_INTERRUPT_ID__MASK 0x3FF
#define GICC_IAR_CPUID__SHIFT       10
#define GICC_IAR_CPUID__MASK        (3 << 10)

#define GICC_EOIR                   (GICC_BASE + 0x010)
#define GICC_EOIR_EOIINTID__MASK    0x3FF
#define GICC_EOIR_CPUID__SHIFT      10
#define GICC_EOIR_CPUID__MASK       (3 << 10)

/* 割り込み番号 */
#define GIC_SGI(n)          (n)
#define GIC_PPI(n)          (16 + (n))  // Private per core
#define GIC_SPI(n)          (32 + (n))  // Shared between cores

#define IRQ_UART0           32
#define IRQ_TIMER0          50
#define IRQ_TIMER1          51
#define IRQ_MSGBOX          81
#define IRQ_DMA             82
#define IRQ_SD0             92
#define IRQ_SD1             93
#define IRQ_OTG_EHCI        104
#define IRQ_OTG_OHCI        105
#define IRQ_EHCI0           106
#define IRQ_OHCI0           107
#define IRQ_EMAC            114

#define IRQ_LINES           157

void irq_init(void);
void irq_enable(int i);
void irq_disable(int i);
void irq_register(int i, void (*f)(void *), void *param);
void irq_handler(void/* int user_mode */);
void irq_error(uint64_t type);

#endif
