#ifndef INC_MMU_H
#define INC_MMU_H

/*
 * See Chapter 12 of ARM Cortex-A Series Programmer's Guide for ARMv8-A
 * and Chapter D5 of Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile.
 */

/*
 * A virtual address 'la' has a four-part structure as follows:
 * +-----9-----+-----9-----+-----9-----+-----9-----+---------12---------+
 * |  Level 0  |  Level 1  |  Level 2  |  Level 3  | Offset within Page |
 * |   Index   |   Index   |   Index   |   Index   |                    |
 * +-----------+-----------+-----------+-----------+--------------------+
 *  \PTX(va, 0)/\PTX(va, 1)/\PTX(va, 2)/\PTX(va, 3)/
 */

#define PAGE_MASK       0xfffffffffffff000
#define PAGE_SHIFT      12
#define TABLE_SHIFT     9

#define PGSIZE          4096
#define PGSHIFT         12
#define L0SHIFT         39
#define L1SHIFT         30
#define L2SHIFT         21
#define L3SHIFT         12
#define ENTRYSZ         512

#define PAGE_SIZE               (1 << PAGE_SHIFT)
#define SECTION_SIZE            (1 << SECTION_SHIFT)
#define SUPERSECTION_SIZE       (1 << SUPERSECTION_SHIFT)

#define PTRS_PER_TABLE          (1 << TABLE_SHIFT)      // 512B
#define PGD_SHIFT               L0SHIFT
#define PUD_SHIFT               L1SHIFT
#define PMD_SHIFT               L2SHIFT
#define PG_DIR_SIZE             (4 * PAGE_SIZE)

#define PTX(level, va) (((uint64_t)(va) >> (39 - 9 * level)) & 0x1FF)
#define L0X(va) (((uint64_t)(va) >> L0SHIFT) & 0x1FF)
#define L1X(va) (((uint64_t)(va) >> L1SHIFT) & 0x1FF)
#define L2X(va) (((uint64_t)(va) >> L2SHIFT) & 0x1FF)
#define L3X(va) (((uint64_t)(va) >> L3SHIFT) & 0x1FF)

#define PGROUNDUP(sz)           (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a)          (((a)) & ~(PGSIZE-1))
#define PGALINED(a)             (((uint64_t)(a) & (PGSIZE-1)) == 0UL)

/* Memory region attributes */
#define MT_DEVICE_nGnRnE        0x0
#define MT_NORMAL               0x1
#define MT_NORMAL_NC            0x2
#define MT_DEVICE_nGnRnE_FLAGS  0x00    /* Device nGnRnE */
#define MT_NORMAL_FLAGS         0xFF    /* Normal Inner/Outer Write-back Non-transient RW-Allocate */
#define MT_NORMAL_NC_FLAGS      0x44    /* Normal Inner/Outer Non-cacheable */

#define MAIR_VALUE              ((MT_DEVICE_nGnRnE_FLAGS << (8 * MT_DEVICE_nGnRnE)) | \
                                (MT_NORMAL_FLAGS << (8 * MT_NORMAL)) | \
                                (MT_NORMAL_NC_FLAGS << (8 * MT_NORMAL_NC)))

/* ARMの用語では4KB粒度（PGTableの最低位エントリ）を使用する。
 * つまり、PUDエントリあたり1GB、PMDエントリあたり2MBとなる */
#define MM_TYPE_PAGE_TABLE      0x3
#define MM_TYPE_PAGE            0x3
#define MM_TYPE_BLOCK           0x1         // block -- pointed by either a PUD entry or a PMD entry

/* Access flag (AF) */
#define MM_AF                   (0x1 << 10)   // "indicates when a page or section of memory is accessed for the first time". if 0, exception. to be set by kernel

#define MM_SH_INNER             (0x3 << 8)    // 00: Non-shareable, 10b: outer sharable, 11b: inner sharable

/* Access permission, bit7:6 in descriptor */
#define MM_AP_MASK              (0x3UL << 6)
#define MM_AP_EL1_RW            (0x0 << 6)      // EL0:no access; EL1/2/3: rw
#define MM_AP_RW                (0x1 << 6)      // EL0/1/2/3: rw
#define MM_AP_EL0_RO            (0x2 << 6)      // EL0:ro; EL1/2/3: rw
#define MM_AP_RO                (0x3 << 6)      // EL0/1/2/3: ro

/* "D4.3.3 Memory attribute fields in the VMSAv8-64 translation table format descriptors" */
#define MM_XN                   (1UL<<54)       // execute-never (Unprivileged execute never for EL1/EL0)
#define MM_PXN                  (1UL<<53)       // Privileged execute-never bit (Determines whether the region is executable at EL1)
#define MM_XN_MASK              (0x3UL << 53)

#define MM_ACCESS               (MM_AF | MM_SH_INNER)

#define MMU_FLAGS               (MM_TYPE_BLOCK | (MT_NORMAL << 2) | MM_ACCESS)          // block (eg section) granuarlity, memory
#define MMU_DEVICE_FLAGS        (MM_TYPE_BLOCK | (MT_DEVICE_nGnRnE << 2) | MM_ACCESS)   // block (eg section) granuarlity, devices
#define MMU_PTE_FLAGS           (MM_TYPE_PAGE | (MT_NORMAL << 2) | MM_ACCESS)           // need to be used with MP_AP_xxx, MM_XN

/* PTE flags */
#define PTE_P                   0x1

#define PTE_TABLE       MM_TYPE_PAGE_TABLE  /* Table in level 0/1/2 */
#define PTE_BLOCK       MM_TYPE_BLOCK       /* Block in level 1/2 */
#define PTE_PAGE        MM_TYPE_PAGE        /* Page in level 3 */

#define PTE_KERN        (0 << 6)
#define PTE_USER        (1 << 6)
#define PTE_RW          (0 << 7)    /* 読み書き可 */
#define PTE_RO          (1 << 7)    /* 読み込みのみ可 */
#define PTE_SH          (3 << 8)    /* キャッシュ可能なメモリでインナ共有可 */
#define SH_OUTER        (2 << 8)
#define SH_INNER        (3 << 8)    /* Inner shareable */
#define AF_USED         (1 << 10)
#define PTE_AF          (1 << 10)   /* アクセスフラグ（0だとアクセスフォル発生） P2066 */
#define PTE_NG          (1 << 11)
#define PTE_PXN         MM_PXN      /* EL1以上での実行不可 */
#define PTE_UXN         MM_XN       /* EL0での実行不可 */

// #define PTE_NORMAL_NC   ((MT_NORMAL_NC << 2) | AF_USED | SH_OUTER)
// #define PTE_NORMAL      ((MT_NORMAL << 2) | AF_USED | SH_OUTER)
#define PTE_NORMAL_NC           ((MT_NORMAL_NC << 2) | AF_USED | SH_INNER)
#define PTE_NORMAL              ((MT_NORMAL << 2) | AF_USED | SH_INNER)
#define PTE_DEVICE              ((MT_DEVICE_nGnRnE << 2) | AF_USED)

/* 1GB/2MB block for kernel, and 4KB page for user. */
#define PTE_KDATA               (PTE_KERN | PTE_NORMAL | PTE_BLOCK)
#define PTE_KDEV                (PTE_KERN | PTE_DEVICE | PTE_BLOCK)
#define PTE_UDATA               (PTE_USER | PTE_NORMAL | PTE_PAGE)
// #define PTE_UDATA       (PTE_USER | PTE_NORMAL_NC | PTE_PAGE)
// #define PTE_UDATA       (PTE_USER | PTE_NORMAL | PTE_PAGE | PTE_NG)

// 1GBブロック（Level 1）の記述子（Descriptor）ビットフラグ
#define L1_BLOCK_VALID   0x1
#define L1_BLOCK_ATTR    (1 << 10) // AF (Access Flag) を立てておかないとアクセス時フォルトする
#define L1_BLOCK_NORMAL  (1 << 2)  // MAIRのインデックス1 (Normal Memory)
#define L1_BLOCK_DEVICE  (0 << 2)  // MAIRのインデックス0 (Device Memory)

/* Address in table or block entry, only support 32 bit physical address. */
#define PTE_ADDR(pte)       ((uint64_t)(pte) & ~0xFFFF000000000FFFUL)
#define PTE_FLAGS(pte)      ((uint64_t)(pte) &  0xFFFF000000000FFFUL)
#define PTE_FLAGS_LOW(pte)  ((uint64_t)(pte) &  0x0000000000000FFFUL)
#define PTE_FLAGS_HIGH(pte) ((uint64_t)(pte) &  0xFFFF000000000000UL)

// extract perm bits from pte
#define PTE_TO_PERM(x)      (x & (unsigned long)(MM_AP_MASK | MM_XN_MASK))
// extract pa from pte.
//#define PTE_TO_PA(x)        (((x) >> 12) << 12)
#define PTE_TO_PA(x)        (PTE_ADDR(x))

/* Translation Control Register */
#define TCR_T0SZ        (64 - 48)
#define TCR_T1SZ        ((64 - 48) << 16)
#define TCR_TG0_4K      (0 << 14)
#define TCR_TG1_4K      (2 << 30)           /* Different from TG0 */
#define TCR_FLAGS       (TCR_T0SZ | TCR_T1SZ | TCR_TG0_4K | TCR_TG1_4K)
#define TCR_SH0_INNER   (3 << 12)
#define TCR_SH1_INNER   (3 << 28)
#define TCR_SH0_OUTER   (2 << 12)
#define TCR_SH1_OUTER   (2 << 28)
#define TCR_ORGN0_IRGN0 ((1 << 10) | (1 << 8))
#define TCR_ORGN1_IRGN1 ((1 << 26) | (1 << 24))

//#define TCR_SH0_OUTER   (0 << 12)
//#define TCR_SH1_OUTER   (0 << 28)
//#define TCR_ORGN0_IRGN0 ((0 << 10) | (0 << 8))
//#define TCR_ORGN1_IRGN1 ((0 << 26) | (0 << 24))

#define TCR_VALUE       (TCR_T0SZ           | TCR_T1SZ      |   \
                         TCR_TG0_4K         | TCR_TG1_4K    |   \
                         TCR_SH0_INNER      | TCR_SH1_INNER |   \
                         TCR_ORGN0_IRGN0    | TCR_ORGN1_IRGN1)


#endif
