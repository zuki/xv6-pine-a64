#ifndef INC_MEMLAYOUT_H
#define INC_MEMLAYOUT_H

#include <param.h>

// ---------------- memory layout --------------------------- //
#define PHYS_BASE 0x40000000UL              /* 物理メモリ最下位アドレス */
#define PHYS_TOP  0x80000000UL              /* 物理メモリ最上位アドレス : 1GB */
#define PHYS_SIZE 0x40000000UL

#define KERN_BASE   VA_START                /* カーネル仮想アドレス開始アドレス */
#define KERN_LINK   (KERN_BASE + 0x42000000)    /* カーネルリンクアドレス */
#define DEVICE_BASE (KERN_BASE + 0x00000000)    /* デバイス基底アドレス  */
#define DEVICE_TOP  (KERN_BASE + 0x02000000)    /* デバイス最上位アドレス */

/*
 * カーネルメモリレイアウト:
 * ```                                              物理アドレス        仮想アドレス
 *  PHYS_TOP       -> ----------------------------  0x8000_0000
 *
 *
 *                           heap                   RW-
 *  PAGE_START    -> ----------------------------   0x4a40_0000
 *                          pages[PAGE_NUM]         RW-
 *                      PAGE_NUM = 0x35c00
 *  *pages        -> ----------------------------   0x4a25_2000
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
 *  USER_TOP    -> ---------------------------   0x0800_0000     (128MB)
 *                        スタック (1MB)
 *
 *  USER_MAX_STACK -> ---------------------------   0x0500_0000　<- sp
 *                        ガードページ (4KB)
 *                    ---------------------------
 *
 *                         ヒープ領域
 *                       ガードページ (4KB)
 *                    ---------------------------
 *                         コード領域
 *  USER_BASE -> ---------------------------    0x0000_0000
 */

//#define STACKPAGE   1024
//#define STACKTOP    USER_TOP                 /* スタックはUSER_TOPから */
//#define STACKBASE   (STACKTOP - STACKPAGE * PGSIZE) /* スタックは16ページ */
//#define MMAPBASE    0x0000FFDFFFFF0000      /* mmapアドレスの基底アドレス */
//#define ELF_ET_DYN_BASE 0x0000FFDEFFFF0000  /* dynamic lib を置くアドレス */
//#define MMAPTOP     USER_TOP                 /* ARGV, ENVPなどはstack_topからmmapするため */


#endif
