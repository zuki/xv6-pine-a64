#ifndef INC_MEMLAYOUT_H
#define INC_MEMLAYOUT_H

#include <param.h>

// ---------------- memory layout --------------------------- //
#define PHYS_BASE 0x40000000                /* 物理メモリ最下位アドレス */
#define PHYS_TOP  0x80000000                /* 物理メモリ最上位アドレス : 1GB */
#define PHYS_SIZE 0x40000000

#define KERN_BASE VA_START      /* カーネル仮想アドレス開始アドレス */
#define KERN_LINK (KERN_BASE+0x42000000)    /* カーネルリンクアドレス */

#define DEVICE_BASE 0x00000000              /* デバイス基底アドレス  */
#define DEVICE_TOP  0x02000000              /* デバイス最上位アドレス */

/*
 * カーネルメモリレイアウト:
 * ```                                              物理アドレス        仮想アドレス
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
 *  KERN_LINK      -> ----------------------------   0x4200_0000        0xFFFF0000_4200_0000
 *
 *  PHYS_BASE      -> ----------------------------   0x4000_0000
 *                         (未使用)
 *  DEVICE_TOP     -> -----------------------------  0x0200_0000
 *                          MMIO
 *  KERN_BASE      -> ----------------------------   0x0000_0000        0xFFFF0000_0000_0000
 * (DEVICE_BASE)
 */

/* ユーザメモリレイアウト.
 *  USER_TOP       -> ---------------------------   0x0001_0000_0000_0000
 *                        スタック (1MB)
 *
 *  STACK_BASE     -> ---------------------------   0x0000_FFFF_FFFF_0000
 *
 *                         mmap領域 (128GB)
 *  MMAPBASE       -> ---------------------------   0x0000_FFDF_FFFF_0000
 *
 *                         libc.so (4GB)
 *  ELF_ET_DYN_BASE  -> --------------------------- 0x0000_FFDE_FFFF_0000
 *
 *                         ヒープ領域
 *                       ガードページ (4KB)
 *                    ---------------------------
 *                         コード領域
 *                           (_start)               0x0040_0000
 *  USER_BASE      -> ---------------------------   0x0000_0000
 */

#define USER_BASE       0x0
#define USER_TOP        0x0001000000000000      // == ユーザスタックのトップ。ユーザ仮想アドレス
#define MMAP_BASE       0x0000FFDFFFFF0000      // mmapアドレスの基底アドレス
#define ELF_ET_DYN_BASE 0x0000FFDEFFFF0000      // dynamic lib を置くアドレス

#define V2P_WO(x) ((x) - KERN_BASE)      /* Same as V2P, but without casts */
#define P2V_WO(x) ((x) + KERN_BASE)      /* Same as P2V, but without casts */

#endif
