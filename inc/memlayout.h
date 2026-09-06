#ifndef INC_MEMLAYOUT_H
#define INC_MEMLAYOUT_H

#define PHYSBTM  0x40000000             /* 物理メモリ最下位アドレス */
#define PHYSTOP  0x80000000             /* 物理メモリ最上位アドレス : 1GB */

#define KERNBASE 0x0UL                  /* カーネル仮想アドレス開始アドレス */
#define KERNLINK (KERNBASE+0x42000000)  /* カーネルリンクアドレス */

#define V2P_WO(x) ((x) - KERNBASE)      /* Same as V2P, but without casts */
#define P2V_WO(x) ((x) + KERNBASE)      /* Same as P2V, but without casts */

#ifndef __ASSEMBLER__

#include <types.h>
#define V2P(a) (((uint64_t) (a)) - KERNBASE)
#define P2V(a) ((void *)(((char *) (a)) + KERNBASE))

#endif

#endif
