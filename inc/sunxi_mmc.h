/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Aaron <leafy.myeh@allwinnertech.com>
 *
 * MMC register definition for allwinner sunxi platform.
 */

#ifndef INC_SUNXI_MMC_H
#define INC_SUNXI_MMC_H

#include <types.h>

struct sunxi_mmc {
    u32 gctrl;      /* 0x00 _CTRL    : 制御レジスタ */
    u32 clkcr;      /* 0x04 _CLKDIV  : クロック制御レジスタ */
    u32 timeout;    /* 0x08 _TMOUT   : タイムアウトレジスタ */
    u32 width;      /* 0x0c _CTYPE   : バス幅レジスタ */
    u32 blksz;      /* 0x10 _BLKSIZ  : ブロックサイズレジスタ */
    u32 bytecnt;    /* 0x14 _BYTCNT  : バイトカウントレジスタ */
    u32 cmd;        /* 0x18 _CMD     : コマンドレジスタ */
    u32 arg;        /* 0x1c _CMDARG  : コマンド引数レジスタ */
    u32 resp0;      /* 0x20 _RESP0   : レスポンス0レジスタ */
    u32 resp1;      /* 0x24 _RESP1   : レスポンス1レジスタ */
    u32 resp2;      /* 0x28 _RESP2   : レスポンス2レジスタ */
    u32 resp3;      /* 0x2c _RESP3   : レスポンス3レジスタ */
    u32 imask;      /* 0x30 _INTMASK : 割り込みマスクレジスタ */
    u32 mint;       /* 0x34 _MINTSTS : マスクされた割り込みステータスレジスタ */
    u32 rint;       /* 0x38 _RINTSTS : 生の割り込みステータスレジスタ */
    u32 status;     /* 0x3c _STATUS  : ステータスレジスタ */
    u32 ftrglevel;  /* 0x40 _FIFOTH  : FIFO水位閾値レジスタ */
    u32 funcsel;    /* 0x44 _FUNS    : FIFO機能選択レジスタ */
    u32 cbcr;       /* 0x48 _TBC0    : コントローラとカード間の転送済みバイトカウント0 */
    u32 bbcr;       /* 0x4c _TBC1    : ホストメモリと内部FIFO間の転送済みバイトカウント1 */
    u32 dbgc;       /* 0x50          : デバッグイネーブルレジスタ */
    u32 res0;       /* 0x54          : 予約 */
    u32 a12a;       /* 0x58 _A12A    : 自動コマンド12引数レジスタ */
    u32 ntsr;       /* 0x5c _NTSR    : SD新規タイミングセットレジスタ */
    u32 res1[8];
    u32 dmac;       /* 0x80 _DMAC    : DMA制御レジスタ */
    u32 dlba;       /* 0x84 _DLBA    : ディスクリプタリスト基底アドレスレジスタ */
    u32 idst;       /* 0x88 _IDST    : DMACステータスレジスタ */
    u32 idie;       /* 0x8c _IDIE    : DMAC割り込みイネーブルレジスタ */
    u32 chda;       /* 0x90 */
    u32 cbda;       /* 0x94 */
    u32 res2[26];
    u32 res3[17];
    u32 samp_dl;
    u32 res4[46];
    u32 fifo;       /* 0x100 / 0x200 FIFO access address */
};

#define SUNXI_MMC_CLK_POWERSAVE         (0x1 << 17)
#define SUNXI_MMC_CLK_ENABLE            (0x1 << 16)
#define SUNXI_MMC_CLK_DIVIDER_MASK      (0xff)

#define SUNXI_MMC_GCTRL_SOFT_RESET      (0x1 << 0)
#define SUNXI_MMC_GCTRL_FIFO_RESET      (0x1 << 1)
#define SUNXI_MMC_GCTRL_DMA_RESET       (0x1 << 2)
#define SUNXI_MMC_GCTRL_RESET           (SUNXI_MMC_GCTRL_SOFT_RESET|\
                        SUNXI_MMC_GCTRL_FIFO_RESET|\
                        SUNXI_MMC_GCTRL_DMA_RESET)
#define SUNXI_MMC_GCTRL_DMA_ENABLE      (0x1 << 5)
#define SUNXI_MMC_GCTRL_ACCESS_BY_AHB   (0x1 << 31)

#define SUNXI_MMC_CMD_RESP_EXPIRE       (0x1 << 6)
#define SUNXI_MMC_CMD_LONG_RESPONSE     (0x1 << 7)
#define SUNXI_MMC_CMD_CHK_RESPONSE_CRC  (0x1 << 8)
#define SUNXI_MMC_CMD_DATA_EXPIRE       (0x1 << 9)
#define SUNXI_MMC_CMD_WRITE             (0x1 << 10)
#define SUNXI_MMC_CMD_AUTO_STOP         (0x1 << 12)
#define SUNXI_MMC_CMD_WAIT_PRE_OVER     (0x1 << 13)
#define SUNXI_MMC_CMD_SEND_INIT_SEQ     (0x1 << 15)
#define SUNXI_MMC_CMD_UPCLK_ONLY        (0x1 << 21)
#define SUNXI_MMC_CMD_START             (0x1 << 31)

#define SUNXI_MMC_RINT_RESP_ERROR       (0x1 << 1)
#define SUNXI_MMC_RINT_COMMAND_DONE     (0x1 << 2)
#define SUNXI_MMC_RINT_DATA_OVER        (0x1 << 3)
#define SUNXI_MMC_RINT_TX_DATA_REQUEST  (0x1 << 4)
#define SUNXI_MMC_RINT_RX_DATA_REQUEST  (0x1 << 5)
#define SUNXI_MMC_RINT_RESP_CRC_ERROR   (0x1 << 6)
#define SUNXI_MMC_RINT_DATA_CRC_ERROR   (0x1 << 7)
#define SUNXI_MMC_RINT_RESP_TIMEOUT     (0x1 << 8)
#define SUNXI_MMC_RINT_DATA_TIMEOUT     (0x1 << 9)
#define SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE  (0x1 << 10)
#define SUNXI_MMC_RINT_FIFO_RUN_ERROR   (0x1 << 11)
#define SUNXI_MMC_RINT_HARD_WARE_LOCKED (0x1 << 12)
#define SUNXI_MMC_RINT_START_BIT_ERROR  (0x1 << 13)
#define SUNXI_MMC_RINT_AUTO_COMMAND_DONE    (0x1 << 14)
#define SUNXI_MMC_RINT_END_BIT_ERROR    (0x1 << 15)
#define SUNXI_MMC_RINT_SDIO_INTERRUPT   (0x1 << 16)
#define SUNXI_MMC_RINT_CARD_INSERT      (0x1 << 30)
#define SUNXI_MMC_RINT_CARD_REMOVE      (0x1 << 31)
#define SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT  \
    (SUNXI_MMC_RINT_RESP_ERROR |            \
     SUNXI_MMC_RINT_RESP_CRC_ERROR |        \
     SUNXI_MMC_RINT_DATA_CRC_ERROR |        \
     SUNXI_MMC_RINT_RESP_TIMEOUT |          \
     SUNXI_MMC_RINT_DATA_TIMEOUT |          \
     SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE |   \
     SUNXI_MMC_RINT_FIFO_RUN_ERROR |        \
     SUNXI_MMC_RINT_HARD_WARE_LOCKED |      \
     SUNXI_MMC_RINT_START_BIT_ERROR |       \
     SUNXI_MMC_RINT_END_BIT_ERROR) /* 0xbfc2 */
#define SUNXI_MMC_RINT_INTERRUPT_DONE_BIT   \
    (SUNXI_MMC_RINT_AUTO_COMMAND_DONE |     \
     SUNXI_MMC_RINT_DATA_OVER |             \
     SUNXI_MMC_RINT_COMMAND_DONE |          \
     SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE)

#define SUNXI_MMC_STATUS_RXWL_FLAG      (0x1 << 0)
#define SUNXI_MMC_STATUS_TXWL_FLAG      (0x1 << 1)
#define SUNXI_MMC_STATUS_FIFO_EMPTY     (0x1 << 2)
#define SUNXI_MMC_STATUS_FIFO_FULL      (0x1 << 3)
#define SUNXI_MMC_STATUS_CARD_PRESENT   (0x1 << 8)
#define SUNXI_MMC_STATUS_CARD_DATA_BUSY (0x1 << 9)
#define SUNXI_MMC_STATUS_DATA_FSM_BUSY  (0x1 << 10)
#define SUNXI_MMC_STATUS_FIFO_LEVEL(reg)    (((reg) >> 17) & 0x3fff)

#define SUNXI_MMC_NTSR_MODE_SEL_NEW     (0x1 << 31)

#define SUNXI_MMC_IDMAC_RESET           (0x1 << 0)
#define SUNXI_MMC_IDMAC_FIXBURST        (0x1 << 1)
#define SUNXI_MMC_IDMAC_ENABLE          (0x1 << 7)

#define SUNXI_MMC_IDIE_TXIRQ            (0x1 << 0)
#define SUNXI_MMC_IDIE_RXIRQ            (0x1 << 1)

#define SUNXI_MMC_COMMON_CLK_GATE       (1 << 16)
#define SUNXI_MMC_COMMON_RESET          (1 << 18)

#define SUNXI_MMC_CAL_DL_SW_EN          (0x1 << 7)

int sunxi_mmc_probe(struct mmc *mmc);
int sunxi_mmc_getcd(struct mmc *mmc);
int sunxi_mmc_set_ios(struct mmc *mmc);
int sunxi_mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd,
                  struct mmc_data *data);

#endif /* _SUNXI_MMC_H */
