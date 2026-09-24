#ifndef INC_SYSREGS_H
#define INC_SYSREGS_H

/* SCTLR_EL1, System Control Register (EL1) */
#define SCTLR_RESERVED                  (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11)
#define SCTLR_EE_LITTLE_ENDIAN          (0 << 25)
#define SCTLR_EOE_LITTLE_ENDIAN         (0 << 24)
#define SCTLR_I_CACHE_DISABLED          (0 << 12)
#define SCTLR_D_CACHE_DISABLED          (0 << 2)
#define SCTLR_MMU_DISABLED              (0 << 0)
#define SCTLR_MMU_ENABLED               (1 << 0)

#define SCTLR_VALUE_MMU_DISABLED    (SCTLR_RESERVED | SCTLR_EE_LITTLE_ENDIAN | SCTLR_I_CACHE_DISABLED | SCTLR_D_CACHE_DISABLED | SCTLR_MMU_DISABLED)

#define SCTLR_VALUE ( SCTLR_RESERVED |  /* mandatory reserved bits, cf the manual */\
                      (1 << 12)  |      /* I, 命令キャッシュを有効化. EL0とEL1における有効化ビット */\
                      (1 << 3)   |      /* SA, スタックアライメントチェックを有効化 */\
                      (1 << 4)   |      /* SA0, EL0でスタックアライメントチェックを有効化 */\
                      (1 << 2)   |      /* C, データキャッシュを有効化. EL0とEL1における有効化ビット*/\
                      (1 << 0) )        /* set M, MMUを有効化 */


/* HCR_EL2, Hypervisor Configuration Register (EL2) */
#define HCR_RW               (1 << 31)
#define HCR_VALUE            HCR_RW

/* SCR_EL3, Secure Configuration Register (EL3) */
#define SCR_RESERVED          (3 << 4)
#define SCR_RW                (1 << 10)
#define SCR_NS                (1 << 0)
#define SCR_VALUE             (SCR_RESERVED | SCR_RW | SCR_NS)

/* SPSR_EL3, Saved Program Status Register (EL3) */
#define SPSR_MASK_ALL        (7 << 6)
#define SPSR_EL1h            (5 << 0)
#define SPSR_EL2h            (9 << 0)
#define SPSR_EL3_VALUE       (SPSR_MASK_ALL | SPSR_EL2h)
#define SPSR_VALUE           (SPSR_MASK_ALL | SPSR_EL1h)

/* EC (Exception Class): ESR_EL1[31:26] */
#define EC_SHIFT                    26
#define EC_UNKNOWN                  0x00
#define EC_SVC64                    0x15
#define EC_IABORT                   0x20
#define EC_IABORT2                  0x21
#define EC_ALIGNMENT                0x22
#define EC_DABORT                   0x24
#define EC_DABORT2                  0x25

/* ISS (nstruction Specific Syndrome): ESR_EL1[24:0] */
#define ISS_MASK                    0x1FFFFFF
/* IR (Instruction Length): ESR_EL1[25] */
#define IL_MASK                     (1 << 25)

#endif
