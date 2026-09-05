#ifndef INC_PERIPHERALS_GPIO_H
#define INC_PERIPHERALS_GPIO_H

#include <peripherals/base.h>

#define PIOX_BASE       (MMIO_BASE + 0x01C20800)
#define PIOS_BASE       (MMIO_BASE + 0x01F02C00)

#define Pn_CFG0(n)      (PIOX_BASE + n * 0x24 + 0x00)
#define Pn_CFG1(n)      (PIOX_BASE + n * 0x24 + 0x04)
#define Pn_CFG2(n)      (PIOX_BASE + n * 0x24 + 0x08)
#define Pn_CFG3(n)      (PIOX_BASE + n * 0x24 + 0x0C)
#define Pn_DAT(n)       (PIOX_BASE + n * 0x24 + 0x10)
#define Pn_DRV0(n)      (PIOX_BASE + n * 0x24 + 0x14)
#define Pn_DRV1(n)      (PIOX_BASE + n * 0x24 + 0x18)
#define Pn_PUL0(n)      (PIOX_BASE + n * 0x24 + 0x1C)
#define Pn_PUL1(n)      (PIOX_BASE + n * 0x24 + 0x20)

#define Pn_EINT_CFG0(m) (PIOX_BASE + 0x200 + m * 0x20 + 0x00)
#define Pn_EINT_CFG1(m) (PIOX_BASE + 0x200 + m * 0x20 + 0x04)
#define Pn_EINT_CFG2(m) (PIOX_BASE + 0x200 + m * 0x20 + 0x08)
#define Pn_EINT_CFG3(m) (PIOX_BASE + 0x200 + m * 0x20 + 0x0C)
#define Pn_EINT_CTL(m)  (PIOX_BASE + 0x200 + m * 0x20 + 0x10)
#define Pn_EINT_STA(m)  (PIOX_BASE + 0x200 + m * 0x20 + 0x14)
#define Pn_EINT_DEB(m)  (PIOX_BASE + 0x200 + m * 0x20 + 0x18)

#define PL_CFG0         (PIOS_BASE + 0x00)
#define PL_CFG1         (PIOS_BASE + 0x04)
#define PL_CFG2         (PIOS_BASE + 0x08)
#define PL_CFG3         (PIOS_BASE + 0x0C)
#define PL_DAT          (PIOS_BASE + 0x10)
#define PL_DRV0         (PIOS_BASE + 0x14)
#define PL_DRV1         (PIOS_BASE + 0x18)
#define PL_PUL0         (PIOS_BASE + 0x1C)
#define PL_PUL1         (PIOS_BASE + 0x20)

#define PL_EINT_CFG0    (PIOS_BASE + 0x200 + 0x00)
#define PL_EINT_CFG1    (PIOS_BASE + 0x200 + 0x04)
#define PL_EINT_CFG2    (PIOS_BASE + 0x200 + 0x08)
#define PL_EINT_CFG3    (PIOS_BASE + 0x200 + 0x0C)
#define PL_EINT_CTL     (PIOS_BASE + 0x200 + 0x10)
#define PL_EINT_STA     (PIOS_BASE + 0x200 + 0x14)
#define PL_EINT_DEB     (PIOS_BASE + 0x200 + 0x18)

#endif
