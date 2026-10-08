// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Aaron <leafy.myeh@allwinnertech.com>
 *
 * MMC driver for allwinner sunxi platform.
 *
 * このドライバは、レガシーMMCインタフェースを使用する(ARM) SPLと
 * 完全なDMインタフェースを使用するU-Boot本体（proper）の双方で
 * 使用される。実際のハードウェア・アクセス・コードは共通であり、
 * このファイルの冒頭に配置されている。レガシーMMCインタフェースの実装が
 * その後に続き、最後にDM_MMCの実装が配置されている。
 */

#include <types.h>
#include <arm.h>
#include <peripherals/gpio.h>
#include <mmc.h>
#include <sunxi_mmc.h>
#include <kalloc.h>
#include <linux/errno.h>
#include <console.h>
#include <timer.h>

#ifndef CCM_MMC_CTRL_MODE_SEL_NEW
#define CCM_MMC_CTRL_MODE_SEL_NEW    0
#endif

#define SMHCO_BASE              (MMIO_BASE + 0x01C0F000)
#define CCU_BASE                (MMIO_BASE + 0x01C20000)
#define PLL_PERIPH0_CTRL_REG    (CCU_BASE + 0x0028)
#define PLL_PERIPH1_CTRL_REG    (CCU_BASE + 0x002C)
#define BUS_CLK_GATING_REG0     (CCU_BASE + 0x0060)
#define SDMMC0_CLK_REG          (CCU_BASE + 0x0088)
#define BUS_SOFT_RST_REG0       (CCU_BASE + 0x02C0)

// sunxi_mmcの各種情報を取りまとめる構造体
struct sunxi_mmc_plat {
    struct mmc_config cfg;
    struct mmc mmc;
};

// sunsi_mmc独自の情報を格納する構造体
struct sunxi_mmc_priv {
    unsigned mmc_no;            // mmc番号 (0)
    uint32_t *mclkreg;          // SDMMC0_CLK_REG
    unsigned fatal_err;
    struct sunxi_mmc *reg;      // sunxi mmcレジスタ構造体
    struct mmc_config cfg;      // 構成情報構造体               FIXME: いるか?
};

static struct sunxi_mmc_priv mmc_priv;
static struct mmc_config mmc_config;

#define sunxi_get32(r)          (get32((uintptr_t)(r)))
#define sunxi_put32(r, v)       (put32((uintptr_t)(r), (v)))

/*
 * A64以降のすべてのMMCコントローラは、自動キャリブレーション
 * 機能を備えている。通常、これは互換性文字列（compatible string）から
 * 検出できるが、SPLでも動作する仕組みが必要である。
 */
static bool sunxi_mmc_can_calibrate(void)
{
    return true;
}

static unsigned int clock_get_pll6(void)
{
    uint32_t rval = sunxi_get32(PLL_PERIPH0_CTRL_REG);
    int n = ((rval & (0x1f << 8)) >> 8) + 1;
    int k = ((rval & (03 << 4)) >> 4) + 1;
    return 24000000 * n * k / 2;
}

// モジュールクロックを設定する
static int mmc_set_mod_clk(struct sunxi_mmc_priv *priv, unsigned int hz)
{
    unsigned int pll, pll_hz, div, n, oclk_dly, sclk_dly;
    uint32_t val = 0;

    if (hz <= 24000000) {
        pll = (0x0 << 24);      // CCM_MMC_CTRL_OSCM24;
        pll_hz = 24000000;
    } else {
        pll = (0x1 << 24);
        pll_hz = clock_get_pll6();
    }

    div = pll_hz / hz;
    if (pll_hz % hz)
        div++;

    n = 0;
    while (div > 16) {
        n++;
        div = (div + 1) / 2;
    }

    if (n > 3) {
        error("mmc %u error cannot set clock to %u", priv->mmc_no,
               hz);
        return -1;
    }

    /* determine delays */
    if (hz <= 400000) {
        oclk_dly = 0;
        sclk_dly = 0;
    } else if (hz <= 25000000) {
        oclk_dly = 0;
        sclk_dly = 5;
    } else {
        if (hz <= 52000000)
            oclk_dly = 3;
        else
            oclk_dly = 1;
        sclk_dly = 4;
    }

    if (!sunxi_mmc_can_calibrate()) {
        val = (oclk_dly << 8) | (sclk_dly << 20);
    }

    // New mode of Samplin/Output Timing
    val |= CCM_MMC_CTRL_MODE_SEL_NEW;
    sunxi_put32(&priv->reg->ntsr, sunxi_get32(&priv->reg->ntsr) | SUNXI_MMC_NTSR_MODE_SEL_NEW);

    //                   clock ON      OSC24M  CLK_DIV_RATION_N CLK_DIV_RATIO_M
    sunxi_put32(priv->mclkreg, (0x1 << 31) | pll | ((n) << 16) | (div - 1) | val);

    debug("mmc %u set mod-clk req %u parent %u n %u m %u rate %u",
          priv->mmc_no, hz, pll_hz, 1u << n, div, pll_hz / (1u << n) / div);

    return 0;
}

static int mmc_update_clk(struct sunxi_mmc_priv *priv)
{
    unsigned int cmd;
    unsigned timeout_msecs = 2000;
    unsigned long start = get_timer(0);

    cmd = SUNXI_MMC_CMD_START |
          SUNXI_MMC_CMD_UPCLK_ONLY |
          SUNXI_MMC_CMD_WAIT_PRE_OVER;

    sunxi_put32(&priv->reg->cmd, cmd);
    while (sunxi_get32(&priv->reg->cmd) & SUNXI_MMC_CMD_START) {
        if (get_timer(start) > timeout_msecs)
            return -1;
    }

    // クロックの更新は様々な割り込みステータスビットをセットするのでこれらをクリアする
    sunxi_put32(&priv->reg->rint, sunxi_get32(&priv->reg->rint));

    return 0;
}

static int mmc_config_clock(struct sunxi_mmc_priv *priv, struct mmc *mmc)
{
    unsigned rval = sunxi_get32(&priv->reg->clkcr);

    // クロックを無効にする
    rval &= ~SUNXI_MMC_CLK_ENABLE;
    sunxi_put32(&priv->reg->clkcr, rval);
    if (mmc_update_clk(priv))
        return -1;

    // mod_clkに新しいレートをセットする
    if (mmc_set_mod_clk(priv, mmc->clock))
        return -1;

    // 内部ディバイダをクリアする
    rval &= ~SUNXI_MMC_CLK_DIVIDER_MASK;
    sunxi_put32(&priv->reg->clkcr, rval);

    /* A64はMMCコントローラの遅延（ディレイ）のキャリブレーションを
     * サポートしている。キャリブレーションを開始する前には遅延をゼロに
     * 設定する必要がある。AllwinnerのBSPドライバは、HS400を使用する
     * 場合にしか遅延を設定しないが、HS400は現時点ではメインラインの
     * U-BootやLinuxではサポートされていない。
     */
    if (sunxi_mmc_can_calibrate())
        sunxi_put32(&priv->reg->samp_dl, SUNXI_MMC_CAL_DL_SW_EN);

    /* クロックを有効にする */
    rval |= SUNXI_MMC_CLK_ENABLE;
    sunxi_put32(&priv->reg->clkcr, rval);
    if (mmc_update_clk(priv))
        return -1;

    return 0;
}

static int sunxi_mmc_set_ios_common(struct sunxi_mmc_priv *priv,
                    struct mmc *mmc)
{
    trace("start with: bus_width: %x, clock: %d",
          mmc->bus_width, mmc->clock);

    /* クロックを変更 */
    if (mmc->clock && mmc_config_clock(priv, mmc) != 0) {
        priv->fatal_err = 1;
        return -EINVAL;
    }

    /* バス幅を変更する */
    if (mmc->bus_width == 8)
        sunxi_put32(&priv->reg->width, 0x2);
    else if (mmc->bus_width == 4)
        sunxi_put32(&priv->reg->width, 0x1);
    else
        sunxi_put32(&priv->reg->width, 0x0);

    return 0;
}

static int mmc_trans_data_by_cpu(struct sunxi_mmc_priv *priv, struct mmc *mmc,
                 struct mmc_data *data)
{
    const int reading = !!(data->flags & MMC_DATA_READ);
    const uint32_t status_bit = reading ? SUNXI_MMC_STATUS_FIFO_EMPTY :
                          SUNXI_MMC_STATUS_FIFO_FULL;
    unsigned i;
    unsigned *buff = (unsigned int *)(reading ? data->dest : data->src);
    unsigned word_cnt = (data->blocksize * data->blocks) >> 2;
    unsigned timeout_msecs = word_cnt >> 6;
    uint32_t status;
    unsigned long  start;

    if (timeout_msecs < 2000)
        timeout_msecs = 2000;

    /* Always read / write data through the CPU */
    sunxi_put32(&priv->reg->gctrl, sunxi_get32(&priv->reg->gctrl) | SUNXI_MMC_GCTRL_ACCESS_BY_AHB);

    start = get_timer(0);

    for (i = 0; i < word_cnt;) {
        unsigned int in_fifo;

        while ((status = sunxi_get32(&priv->reg->status)) & status_bit) {
            if (get_timer(start) > timeout_msecs)
                return -1;
        }

        /*
         * For writing we do not easily know the FIFO size, so have
         * to check the FIFO status after every word written.
         * TODO: For optimisation we could work out a minimum FIFO
         * size across all SoCs, and use that together with the current
         * fill level to write chunks of words.
         */
        if (!reading) {
            sunxi_put32(&priv->reg->fifo, buff[i++]);
            continue;
        }

        /*
         * The status register holds the current FIFO level, so we
         * can be sure to collect as many words from the FIFO
         * register without checking the status register after every
         * read. That saves half of the costly MMIO reads, effectively
         * doubling the read performance.
         * Some SoCs (A20) report a level of 0 if the FIFO is
         * completely full (value masked out?). Use a safe minimal
         * FIFO size in this case.
         */
        in_fifo = SUNXI_MMC_STATUS_FIFO_LEVEL(status);
        if (in_fifo == 0 && (status & SUNXI_MMC_STATUS_FIFO_FULL))
            in_fifo = 32;
        for (; in_fifo > 0; in_fifo--)
            buff[i++] = sunxi_get32(&priv->reg->fifo);
        dmb();
    }

    return 0;
}

static int mmc_rint_wait(struct sunxi_mmc_priv *priv, struct mmc *mmc,
             uint timeout_msecs, uint done_bit, const char *what)
{
    unsigned int status;
    unsigned long start = get_timer(0);

    do {
        status = sunxi_get32(&priv->reg->rint);
        if ((get_timer(start) > timeout_msecs) ||
            (status & SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT)) {
            debug("%s timeout %x", what,
                  status & SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT);
            return -ETIMEDOUT;
        }
    } while (!(status & done_bit));

    return 0;
}

static int sunxi_mmc_send_cmd_common(struct sunxi_mmc_priv *priv,
                     struct mmc *mmc, struct mmc_cmd *cmd,
                     struct mmc_data *data)
{
    unsigned int cmdval = SUNXI_MMC_CMD_START;
    unsigned int timeout_msecs;
    int error = 0;
    unsigned int status = 0;
    unsigned int bytecnt = 0;

    if (priv->fatal_err)
        return -1;
    if (cmd->resp_type & MMC_RSP_BUSY)
        debug("mmc cmd %d check rsp busy", cmd->cmdidx);
    if (cmd->cmdidx == 12)      // 転送中止
        return 0;

    if (!cmd->cmdidx)
        cmdval |= SUNXI_MMC_CMD_SEND_INIT_SEQ;
    if (cmd->resp_type & MMC_RSP_PRESENT)
        cmdval |= SUNXI_MMC_CMD_RESP_EXPIRE;
    if (cmd->resp_type & MMC_RSP_136)
        cmdval |= SUNXI_MMC_CMD_LONG_RESPONSE;
    if (cmd->resp_type & MMC_RSP_CRC)
        cmdval |= SUNXI_MMC_CMD_CHK_RESPONSE_CRC;

    if (data) {
        if ((uint32_t)(long)data->dest & 0x3) {
            error = -1;
            goto out;
        }

        cmdval |= SUNXI_MMC_CMD_DATA_EXPIRE|SUNXI_MMC_CMD_WAIT_PRE_OVER;
        if (data->flags & MMC_DATA_WRITE)
            cmdval |= SUNXI_MMC_CMD_WRITE;
        if (data->blocks > 1)
            cmdval |= SUNXI_MMC_CMD_AUTO_STOP;
        sunxi_put32(&priv->reg->blksz, data->blocksize);
        sunxi_put32(&priv->reg->bytecnt, data->blocks * data->blocksize);
    }

    debug("mmc %d, cmd %d(0x%08x), arg 0x%08x", priv->mmc_no,
          cmd->cmdidx, cmdval | cmd->cmdidx, cmd->cmdarg);
    sunxi_put32(&priv->reg->arg, cmd->cmdarg);

    if (!data)
        sunxi_put32(&priv->reg->cmd, cmdval | cmd->cmdidx);

    /*
     * transfer data and check status
     * STATREG[2] : FIFO empty
     * STATREG[3] : FIFO full
     */
    if (data) {
        int ret = 0;

        bytecnt = data->blocksize * data->blocks;
        debug("trans data %d bytes", bytecnt);
        sunxi_put32(&priv->reg->cmd, cmdval | cmd->cmdidx);
        ret = mmc_trans_data_by_cpu(priv, mmc, data);
        if (ret) {
            error = sunxi_get32(&priv->reg->rint) &
                SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT;
            error = -ETIMEDOUT;
            goto out;
        }
    }

    error = mmc_rint_wait(priv, mmc, 1000, SUNXI_MMC_RINT_COMMAND_DONE,
                  "cmd");
    if (error)
        goto out;

    if (data) {
        timeout_msecs = 120;
        debug("cacl timeout %x msec", timeout_msecs);
        error = mmc_rint_wait(priv, mmc, timeout_msecs,
                      data->blocks > 1 ?
                      SUNXI_MMC_RINT_AUTO_COMMAND_DONE :
                      SUNXI_MMC_RINT_DATA_OVER,
                      "data");
        if (error)
            goto out;
    }

    if (cmd->resp_type & MMC_RSP_BUSY) {
        unsigned long start = get_timer(0);
        timeout_msecs = 2000;

        do {
            status = sunxi_get32(&priv->reg->status);
            if (get_timer(start) > timeout_msecs) {
                debug("busy timeout");
                error = -ETIMEDOUT;
                goto out;
            }
        } while (status & SUNXI_MMC_STATUS_CARD_DATA_BUSY);
    }

    if (cmd->resp_type & MMC_RSP_136) {
        cmd->response[0] = sunxi_get32(&priv->reg->resp3);
        cmd->response[1] = sunxi_get32(&priv->reg->resp2);
        cmd->response[2] = sunxi_get32(&priv->reg->resp1);
        cmd->response[3] = sunxi_get32(&priv->reg->resp0);
        debug("mmc resp 0x%08x 0x%08x 0x%08x 0x%08x",
              cmd->response[3], cmd->response[2],
              cmd->response[1], cmd->response[0]);
    } else {
        cmd->response[0] = sunxi_get32(&priv->reg->resp0);
        debug("mmc resp 0x%08x", cmd->response[0]);
    }
out:
    if (error < 0) {
        debug("error: %d", error);
        sunxi_put32(&priv->reg->gctrl, SUNXI_MMC_GCTRL_RESET);
        mmc_update_clk(priv);
    }

    sunxi_put32(&priv->reg->rint, 0xffffffff);
    sunxi_put32(&priv->reg->gctrl, sunxi_get32(&priv->reg->gctrl) | SUNXI_MMC_GCTRL_FIFO_RESET);
    delayus(1000);

    return error;
}

int sunxi_mmc_set_ios(struct mmc *mmc)
{
    return sunxi_mmc_set_ios_common(mmc->priv, mmc);
}

int sunxi_mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd,
                  struct mmc_data *data)
{
    struct sunxi_mmc_priv *priv = mmc->priv;

    return sunxi_mmc_send_cmd_common(priv, mmc, cmd, data);
}

static void sunxi_cd_gpios_init(void)
{
    // ポートFはn=5
    // 1. PF6ピンを「入力」に設定
    uint32_t cfg = sunxi_get32(Pn_CFG0(5));
    cfg &= ~(0x7 << 24); // 24-26ビットをクリア (0x0 = Input)
    sunxi_put32(Pn_CFG0(5), cfg);

    // 2. PF6ピンを「プルアップ」に設定
    uint32_t pull = sunxi_get32(Pn_PUL0(5));
    pull &= ~(0x3 << 12); // 12-13ビットをクリア
    pull |=  (0x1 << 12); // Pull-up をセット
    sunxi_put32(Pn_PUL0(5), pull);

    // 3. レジスタ設定が電気的に安定するまでわずかに待つ (数サイクル)
    for (volatile int i = 0; i < 100; i++);
    debug("sunxi_cd_gpios_init ok");
}

// カードが挿入されてれば 1
int sunxi_mmc_getcd(struct mmc *mmc)
{
    // 4. PF_DATA レジスタからピンの値を読み取る（Bit 6 の値を抽出）
    uint32_t data = sunxi_get32(Pn_DAT(5));
    int pin_state = (data >> 6) & 0x1;
    trace("pin_state: 0x%x", pin_state);
    // cd-gpios のフラグが 0x01 (ACTIVE_LOW) なので、
    // 0 (GND) のときに「カードあり(1)」、1 (VCC) のときに「カードなし(0)」を返す
    return (pin_state == 0) ? 1 : 0;
}

int sunxi_mmc_probe(struct mmc *mmc)
{
    struct sunxi_mmc_priv *priv = &mmc_priv;
    struct mmc_config *cfg = &mmc_config;
    int ret;

    cfg->name = "sunxi_mmc";
    cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
    cfg->host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS;
    cfg->b_max = 65535;             // CONFIG_SYS_MMC_MAX_BLK_COUNT
    cfg->f_min = 400000;            // デフォルト値
    cfg->f_max = 150000000;         // 150MHz

    cfg->host_caps |= (MMC_MODE_4BIT | MMC_MODE_1BIT);
    cfg->host_caps |= (UHS_CAPS | MMC_MODE_HS200 |
                       MMC_MODE_HS400 | MMC_MODE_HS400_ES);

    priv->reg = (struct sunxi_mmc *)(uintptr_t)SMHCO_BASE;
    priv->mmc_no = 0;                                       // mmc0
    priv->mclkreg = (uint32_t *)(uintptr_t)SDMMC0_CLK_REG;

#if 0
    debug("PLL_PERIPH1_CTRL_REG: 0x%x", sunxi_get32(PLL_PERIPH1_CTRL_REG));
    debug("BUS_CLK_GATING_REG0: 0x%x", sunxi_get32(BUS_CLK_GATING_REG0));
    debug("BUS_SOFT_RST_REG0: 0x%x", sunxi_get32(BUS_SOFT_RST_REG0));

    // FIXME: これは必要か（u-bootで設定済みのはず）
    sunxi_put32(PLL_PERIPH1_CTRL_REG, sunxi_get32(PLL_PERIPH1_CTRL_REG) | (1 << 31));   // PLL_enable
    sunxi_put32(BUS_CLK_GATING_REG0, sunxi_get32(BUS_CLK_GATING_REG0) | (1 << 8));      // SMHC0クロックゲートを開ける
    sunxi_put32(BUS_SOFT_RST_REG0, sunxi_get32(BUS_SOFT_RST_REG0) | (1 << 8));          // SMHC0をリセット
#endif
    ret = mmc_set_mod_clk(priv, 24000000);
    if (ret)
        return ret;

    // cd-gpios (PF6)の設定
    sunxi_cd_gpios_init();

    mmc->priv = priv;
    mmc->cfg = (const struct mmc_config *)cfg;

    /* コントローラをリセットする */
    sunxi_put32(&priv->reg->gctrl, SUNXI_MMC_GCTRL_RESET);
    delayus(1000);

    trace("SMHC_CNTL: %x", sunxi_get32(&priv->reg->gctrl));

    return 0;
}


