#ifndef INC_MEMLAYOUT_H
#define INC_MEMLAYOUT_H

// ---------------- memory layout --------------------------- //
#define PHYS_BASE 0x40000000                /* 物理メモリ最下位アドレス */
#define PHYS_TOP  0x80000000                /* 物理メモリ最上位アドレス : 1GB */
#define PHYS_SIZE 0x40000000

#define KERN_BASE 0x0UL                     /* カーネル仮想アドレス開始アドレス */
#define KERN_LINK (KERN_BASE+0x42000000)    /* カーネルリンクアドレス */

#define DEVICE_BASE KERN_BASE               /* デバイス基底アドレス  */
#define DEVICE_TOP  0x02000000              /* デバイス最上位アドレス */

/*
 * カーネルメモリレイアウト:
 * ```
 *  PHYS_TOP       -> ----------------------------  0x8000_0000
 *
 *
 *                           heap                   RW-
 *  PAGE_START    -> ----------------------------   0x
 *                          pages[PAGE_NUM]         RW-
 *                      PAGE_NUM = 0x
 *  *pages        -> ----------------------------   0x
 *
 *
 *  (u-boot-end)  -> ----------------------------   0x4a10_0000
 *                          1 MB
 *  (u-boot)      -> ----------------------------   0x4a00_0000
 *
 *  end           -> ----------------------------   0x
 *                    カーネル rodata, data, bss    RW-
 *                   ----------------------------
 *                         カーネル text            R-X
 *  KERN_LINK      -> ----------------------------   0x4200_0000
 *
 *  PHYS_BASE      -> ----------------------------    0x4000_0000
 *                          (未使用)
 *  DEVICE_TOP    -> -----------------------------   0x0200_0000
 *                          MMIO
 *  KERN_BASE      -> ----------------------------   0x0000_0000
 *  (DEVICE_BASE)
 */


#define V2P_WO(x) ((x) - KERN_BASE)      /* Same as V2P, but without casts */
#define P2V_WO(x) ((x) + KERN_BASE)      /* Same as P2V, but without casts */

#ifndef __ASSEMBLER__

#include <types.h>
#define V2P(a) (((uint64_t) (a)) - KERN_BASE)
#define P2V(a) ((void *)(((char *) (a)) + KERN_BASE))

#endif

#endif
