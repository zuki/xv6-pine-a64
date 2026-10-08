// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2008, Freescale Semiconductor, Inc
 * Copyright 2020 NXP
 * Andy Fleming
 *
 * Based vaguely on the Linux code
 */
#include <types.h>
#include <mmc.h>
#include <sunxi_mmc.h>
#include <arm.h>
#include <memalign.h>
#include <linux/errno.h>
#include <console.h>
#include <timer.h>
#include <string.h>
#include <sd.h>

#define DEFAULT_CMD6_TIMEOUT_MS  500

static int mmc_set_signal_voltage(struct mmc *mmc, unsigned int signal_voltage);

void mmmc_trace_before_send(struct mmc *mmc, struct mmc_cmd *cmd)
{
    info("CMD_SEND:%d", cmd->cmdidx);
    cprintf("\t\tARG\t\t\t 0x%08x\n", cmd->cmdarg);
}

void mmmc_trace_after_send(struct mmc *mmc, struct mmc_cmd *cmd, int ret)
{
    int i;
    uint8_t *ptr;

    if (ret) {
        cprintf("\t\tRET\t\t\t %d\n", ret);
    } else {
        switch (cmd->resp_type) {
        case MMC_RSP_NONE:
            cprintf("\t\tMMC_RSP_NONE\n");
            break;
        case MMC_RSP_R1:
            cprintf("\t\tMMC_RSP_R1,5,6,7 \t 0x%08x \n",
                cmd->response[0]);
            break;
        case MMC_RSP_R1b:
            cprintf("\t\tMMC_RSP_R1b\t\t 0x%08x \n",
                cmd->response[0]);
            break;
        case MMC_RSP_R2:
            cprintf("\t\tMMC_RSP_R2\t\t 0x%08x \n",
                cmd->response[0]);
            cprintf("\t\t          \t\t 0x%08x \n",
                cmd->response[1]);
            cprintf("\t\t          \t\t 0x%08x \n",
                cmd->response[2]);
            cprintf("\t\t          \t\t 0x%08x \n",
                cmd->response[3]);
            cprintf("\n");
            cprintf("\t\t\t\t\tDUMPING DATA\n");
            for (i = 0; i < 4; i++) {
                int j;
                cprintf("\t\t\t\t\t%03d - ", i*4);
                ptr = (uint8_t *)&cmd->response[i];
                ptr += 3;
                for (j = 0; j < 4; j++)
                    cprintf("%02x ", *ptr--);
                cprintf("\n");
            }
            break;
        case MMC_RSP_R3:
            cprintf("\t\tMMC_RSP_R3,4\t\t 0x%08x \n",
                cmd->response[0]);
            break;
        default:
            cprintf("\t\tERROR MMC rsp not supported\n");
            break;
        }
    }
}

static void print_csd_info(struct mmc *mmc)
{
    cprintf("== info from csd ===\n");
    cprintf("legacy_speed:  %d\n", mmc->legacy_speed);
    cprintf("dsr_imp:       %d\n", mmc->dsr_imp);
    cprintf("read_bl_len:   %d\n", mmc->read_bl_len);
    cprintf("write_bl_len:  %d\n", mmc->write_bl_len);
    cprintf("capacity_user: 0x%016lx\n", mmc->capacity_user);
    cprintf("version:       0x%08x\n", mmc->version);
    cprintf("card_caps:     0x%08x\n", mmc->card_caps);
}

void mmc_trace_state(struct mmc *mmc, struct mmc_cmd *cmd)
{
    int status;

    status = (cmd->response[0] & MMC_STATUS_CURR_STATE) >> 9;
    info("CURR STATE:%d", status);
}

const char *mmc_mode_name(enum bus_mode mode)
{
    static const char *const names[] = {
          [MMC_LEGACY]      = "MMC legacy",
          [MMC_HS]          = "MMC High Speed (26MHz)",
          [SD_HS]           = "SD High Speed (50MHz)",
          [UHS_SDR12]       = "UHS SDR12 (25MHz)",
          [UHS_SDR25]       = "UHS SDR25 (50MHz)",
          [UHS_SDR50]       = "UHS SDR50 (100MHz)",
          [UHS_SDR104]      = "UHS SDR104 (208MHz)",
          [UHS_DDR50]       = "UHS DDR50 (50MHz)",
          [MMC_HS_52]       = "MMC High Speed (52MHz)",
          [MMC_DDR_52]      = "MMC DDR52 (52MHz)",
          [MMC_HS_200]      = "HS200 (200MHz)",
          [MMC_HS_400]      = "HS400 (200MHz)",
          [MMC_HS_400_ES]   = "HS400ES (200MHz)",
    };

    if (mode >= MMC_MODES_END)
        return "Unknown mode";
    else
        return names[mode];
}

static unsigned int mmc_mode2freq(struct mmc *mmc, enum bus_mode mode)
{
    static const int freqs[] = {
          [MMC_LEGACY]      = 25000000,
          [MMC_HS]          = 26000000,
          [SD_HS]           = 50000000,
          [MMC_HS_52]       = 52000000,
          [MMC_DDR_52]      = 52000000,
          [UHS_SDR12]       = 25000000,
          [UHS_SDR25]       = 50000000,
          [UHS_SDR50]       = 100000000,
          [UHS_DDR50]       = 50000000,
          [UHS_SDR104]      = 208000000,
          [MMC_HS_200]      = 200000000,
          [MMC_HS_400]      = 200000000,
          [MMC_HS_400_ES]   = 200000000,
    };

    if (mode == MMC_LEGACY)
        return mmc->legacy_speed;
    else if (mode >= MMC_MODES_END)
        return 0;
    else
        return freqs[mode];
}

// モードを設定
static int mmc_select_mode(struct mmc *mmc, enum bus_mode mode)
{
    mmc->selected_mode = mode;
    mmc->tran_speed = mmc_mode2freq(mmc, mode);
    mmc->ddr_mode = mmc_is_mode_ddr(mode);
    debug("selecting mode %s (freq : %d MHz)", mmc_mode_name(mode),
         mmc->tran_speed / 1000000);
    return 0;
}

// 一度に処理できる最大ブロック数
static int mmc_get_b_max(struct mmc *mmc, void *dst, lbaint_t blkcnt)
{
    int dst_blk = 4096 / 512;
    return (dst_blk > mmc->cfg->b_max) ? mmc->cfg->b_max : dst_blk;
}

// カードが挿入されているかチェックする
static int mmc_getcd(struct mmc *mmc)
{
    return sunxi_mmc_getcd(mmc);
}

// デバイスのIOをセットする
static int mmc_set_ios(struct mmc *mmc)
{
    return sunxi_mmc_set_ios(mmc);
}

#if 0
// 15. 信号電圧をセットする
static int mmc_set_voltage(struct mmc *mmc)
{
    return 0;
}
#endif

// チューニングを行う
static int mmc_execute_tuning(struct mmc *mmc, unsigned int opcode)
{
    return 0;
}

// コマンドcmdを送信する : dataがあれば送受信する
int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
    int ret;

    //mmmc_trace_before_send(mmc, cmd);
    ret = sunxi_mmc_send_cmd(mmc, cmd, data);
    //mmmc_trace_after_send(mmc, cmd, ret);

    return ret;
}


/**
 * mmc_send_cmd_retry() - MMCデバイスにコマンドを送信する。エラー時は再試行する
 *
 * @dev:    device to receive the command
 * @cmd:    command to send
 * @data:    additional data to send/receive
 * @retries:    how many times to retry; mmc_send_cmd is always called at least
 *              once
 * Return: 0 if ok, -ve on error
 */
static int mmc_send_cmd_retry(struct mmc *mmc, struct mmc_cmd *cmd,
                  struct mmc_data *data, unsigned int retries)
{
    int ret;

    do {
        ret = mmc_send_cmd(mmc, cmd, data);
    } while (ret && retries--);

    return ret;
}

/**
 * mmc_send_cmd_quirks() - MMCデバイスにコマンドを送付する。quirkが有効な場合は、
 *                         それに合わせて再試行する
 *
 * @dev:    device to receive the command
 * @cmd:    command to send
 * @data:    additional data to send/receive
 * @quirk:    retry only if this quirk is enabled
 * @retries:    how many times to retry; mmc_send_cmd is always called at least
 *              once
 * Return: 0 if ok, -ve on error
 */
static int mmc_send_cmd_quirks(struct mmc *mmc, struct mmc_cmd *cmd,
                   struct mmc_data *data, u32 quirk, unsigned int retries)
{
    if (mmc->quirks & quirk)
        return mmc_send_cmd_retry(mmc, cmd, data, retries);
    else
        return mmc_send_cmd(mmc, cmd, data);
}

// 送信状態を取得する : CMD13
int mmc_send_status(struct mmc *mmc, unsigned int *status)
{
    struct mmc_cmd cmd;
    int ret;

    cmd.cmdidx = MMC_CMD_SEND_STATUS;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = mmc->rca << 16;

    ret = mmc_send_cmd_retry(mmc, &cmd, NULL, 4);
    mmc_trace_state(mmc, &cmd);
    if (!ret)
        *status = cmd.response[0];

    return ret;
}

// ビジー状態がクリアするまで待機して、送信ステータスをチェックする
int mmc_poll_for_busy(struct mmc *mmc, int timeout_ms)
{
    unsigned int status;
    int err;

    err = mmc_wait_dat0(mmc, 1, timeout_ms * 1000);
    if (err != -ENOSYS)
        return err;

    while (1) {
        err = mmc_send_status(mmc, &status);
        if (err)
            return err;

        if ((status & MMC_STATUS_RDY_FOR_DATA) &&
            (status & MMC_STATUS_CURR_STATE) !=
             MMC_STATE_PRG)
            break;

        if (status & MMC_STATUS_MASK) {
            error("Status Error: 0x%08x", status);
            return -ECOMM;
        }

        if (timeout_ms-- <= 0)
            break;

        delayus(1000);
    }

    if (timeout_ms <= 0) {
        error("Timeout waiting card ready");
        return -ETIMEDOUT;
    }

    return 0;
}

// DAT0ラインがstateになるのを待つ（この機能はPineA64のSDカードにはなし）
int mmc_wait_dat0(struct mmc *mmc, int state, int timeout_us)
{
    return 0;
}

// ブロック長をセットする : CMD16
int mmc_set_blocklen(struct mmc *mmc, int len)
{
    struct mmc_cmd cmd;

    if (mmc->ddr_mode)
        return 0;

    cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = len;

    return mmc_send_cmd_quirks(mmc, &cmd, NULL,
                   MMC_QUIRK_RETRY_SET_BLOCKLEN, 4);
}

#if 0
// サンプリングポイントのチューニングパターン : バス幅4ビット用
static const uint8_t tuning_blk_pattern_4bit[] = {
    0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
    0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
    0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
    0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
    0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
    0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
    0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
    0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

// サンプリングポイントのチューニングパターン : バス幅8ビット用
static const uint8_t tuning_blk_pattern_8bit[] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
    0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
    0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
    0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
    0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
    0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
    0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
    0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
    0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
    0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
    0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
    0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
    0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
    0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
    0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
    0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

// チューニングパターンの送信
int mmc_send_tuning(struct mmc *mmc, u32 opcode, int *cmd_error)
{
    struct mmc_cmd cmd;
    struct mmc_data data;
    const uint8_t *tuning_block_pattern;
    int size, err;

    if (mmc->bus_width == 8) {
        tuning_block_pattern = tuning_blk_pattern_8bit;
        size = sizeof(tuning_blk_pattern_8bit);
    } else if (mmc->bus_width == 4) {
        tuning_block_pattern = tuning_blk_pattern_4bit;
        size = sizeof(tuning_blk_pattern_4bit);
    } else {
        return -EINVAL;
    }

    ALLOC_CACHE_ALIGN_BUFFER(uint8_t, data_buf, size);

    cmd.cmdidx = opcode;
    cmd.cmdarg = 0;
    cmd.resp_type = MMC_RSP_R1;

    data.dest = (void *)data_buf;
    data.blocks = 1;
    data.blocksize = size;
    data.flags = MMC_DATA_READ;

    err = mmc_send_cmd(mmc, &cmd, &data);
    if (err)
        return err;

    if (memcmp(data_buf, tuning_block_pattern, size))
        return -EIO;

    return 0;
}
#endif

// 転送を停止する : CM12
int mmc_send_stop_transmission(struct mmc *mmc, bool write)
{
    struct mmc_cmd cmd;

    cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
    cmd.cmdarg = 0;
    /*
     * JEDEC Standard No. 84-B51 Page 126
     * CMD12 STOP_TRANSMISSION R1/R1b[3]
     * NOTE 3 R1 for read cases and R1b for write cases.
     *
     * Physical Layer Simplified Specification Version 9.00
     * 7.3.1.3 Detailed Command Description
     * CMD12 R1b
     */
    cmd.resp_type = (IS_SD(mmc) || write) ? MMC_RSP_R1b : MMC_RSP_R1;

    return mmc_send_cmd(mmc, &cmd, NULL);
}

// 22. カードをアイドル状態にする（カードのリセット） : CMD0
static int mmc_go_idle(struct mmc *mmc)
{
    struct mmc_cmd cmd;
    int err;

    delayus(1000);

    cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
    cmd.cmdarg = 0;
    cmd.resp_type = MMC_RSP_NONE;

    err = mmc_send_cmd(mmc, &cmd, NULL);

    if (err)
        return err;

    delayus(2000);

    return 0;
}

// 25. 信号電圧を切替える（3.3v固定）
static int mmc_switch_voltage(struct mmc *mmc, int signal_voltage)
{
    return 0;
}

static int sd_send_op_cond(struct mmc *mmc, bool uhs_en)
{
    int timeout = 1000;
    int err;
    struct mmc_cmd cmd;

    while (1) {
        cmd.cmdidx = MMC_CMD_APP_CMD;           // 55
        cmd.resp_type = MMC_RSP_R1;
        cmd.cmdarg = 0;

        err = mmc_send_cmd(mmc, &cmd, NULL);

        if (err)
            return err;

        cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;   // 41
        cmd.resp_type = MMC_RSP_R3;

        /*
         * ほとんどのカードはOCR内の予約ビットがセットされていても
         * それに応答しない。一部のコントローラはビ​​ット7
         * （低電圧用に予約）をセットすることがあるが、低電圧対応の
         * SDカードの扱いはまだ規定されていない。
         */
        cmd.cmdarg = (mmc->cfg->voltages & 0xff8000);

        if (mmc->version == SD_VERSION_2)
            cmd.cmdarg |= OCR_HCS;

        if (uhs_en)
            cmd.cmdarg |= OCR_S18R;

        err = mmc_send_cmd(mmc, &cmd, NULL);

        if (err)
            return err;

        if (cmd.response[0] & OCR_BUSY)
            break;

        if (timeout-- <= 0)
            return -EOPNOTSUPP;

        delayus(1000);
    }

    if (mmc->version != SD_VERSION_2)
        mmc->version = SD_VERSION_1_0;

    mmc->ocr = cmd.response[0];

    if (uhs_en && (cmd.response[0] & 0x41000000) == 0x41000000) {
        err = mmc_switch_voltage(mmc, MMC_SIGNAL_VOLTAGE_180);
        if (err)
            return err;
    }

    mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
    mmc->rca = 0;

    debug("ocr: 0x%x, hc: 0x%x", mmc->ocr, mmc->high_capacity);

    return 0;
}

static int mmc_send_op_cond_iter(struct mmc *mmc, int use_arg)
{
    struct mmc_cmd cmd;
    int err;

    cmd.cmdidx = MMC_CMD_SEND_OP_COND;
    cmd.resp_type = MMC_RSP_R3;
    cmd.cmdarg = 0;
    if (use_arg)
        cmd.cmdarg = OCR_HCS |
            (mmc->cfg->voltages &
            (mmc->ocr & OCR_VOLTAGE_MASK)) |
            (mmc->ocr & OCR_ACCESS_MODE);

    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err)
        return err;
    mmc->ocr = cmd.response[0];
    return 0;
}

static int mmc_send_op_cond(struct mmc *mmc)
{
    int err, i;
    int timeout = 1000;
    unsigned int start;

    /* Some cards seem to need this */
    mmc_go_idle(mmc);

    start = get_timer(0);
    /* Asking to the card its capabilities */
    for (i = 0; ; i++) {
        err = mmc_send_op_cond_iter(mmc, i != 0);
        if (err)
            return err;

        /* exit if not busy (flag seems to be inverted) */
        if (mmc->ocr & OCR_BUSY)
            break;

        if (get_timer(start) > timeout)
            return -ETIMEDOUT;
        delayus(100);
    }
    mmc->op_cond_pending = 1;
    return 0;
}

//
static int mmc_complete_op_cond(struct mmc *mmc)
{
    int timeout = 1000;
    unsigned long start;
    int err;

    mmc->op_cond_pending = 0;
    if (!(mmc->ocr & OCR_BUSY)) {
        /* Some cards seem to need this */
        mmc_go_idle(mmc);

        start = get_timer(0);
        while (1) {
            err = mmc_send_op_cond_iter(mmc, 1);
            if (err)
                return err;
            if (mmc->ocr & OCR_BUSY)
                break;
            if (get_timer(start) > timeout)
                return -EOPNOTSUPP;
            delayus(100);
        }
    }

    mmc->version = MMC_VERSION_UNKNOWN;

    mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
    mmc->rca = 1;

    return 0;
}


int mmc_send_ext_csd(struct mmc *mmc, uint8_t *ext_csd)
{
    struct mmc_cmd cmd;
    struct mmc_data data;
    int err;

    /* Get the Card Status Register */
    cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = 0;

    data.dest = (char *)ext_csd;
    data.blocks = 1;
    data.blocksize = MMC_MAX_BLOCK_LEN;
    data.flags = MMC_DATA_READ;

    err = mmc_send_cmd(mmc, &cmd, &data);

    return err;
}

static int __mmc_switch(struct mmc *mmc, uint8_t set, uint8_t index, uint8_t value,
            bool send_status)
{
    unsigned int status, start;
    struct mmc_cmd cmd;
    int timeout_ms = DEFAULT_CMD6_TIMEOUT_MS;
    int ret;

    cmd.cmdidx = MMC_CMD_SWITCH;
    cmd.resp_type = MMC_RSP_R1b;
    cmd.cmdarg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                 (index << 16) |
                 (value << 8);

    ret = mmc_send_cmd_retry(mmc, &cmd, NULL, 3);
    if (ret)
        return ret;

    start = get_timer(0);

    /* poll dat0 for rdy/buys status */
    ret = mmc_wait_dat0(mmc, 1, timeout_ms * 1000);
    if (ret && ret != -ENOSYS)
        return ret;

    /*
     * In cases when neiter allowed to poll by using CMD13 nor we are
     * capable of polling by using mmc_wait_dat0, then rely on waiting the
     * stated timeout to be sufficient.
     */
    if (ret == -ENOSYS && !send_status) {
        delayus(timeout_ms * 1000);
        return 0;
    }

    if (!send_status)
        return 0;

    /* Finally wait until the card is ready or indicates a failure
     * to switch. It doesn't hurt to use CMD13 here even if send_status
     * is false, because by now (after 'timeout_ms' ms) the bus should be
     * reliable.
     */
    do {
        ret = mmc_send_status(mmc, &status);

        if (!ret && (status & MMC_STATUS_SWITCH_ERROR)) {
            debug("switch failed %d/%d/0x%x !", set, index,
                 value);
            return -EIO;
        }
        if (!ret && (status & MMC_STATUS_RDY_FOR_DATA) &&
            (status & MMC_STATUS_CURR_STATE) == MMC_STATE_TRANS)
            return 0;
        delayus(100);
    } while (get_timer(start) < timeout_ms);

    return -ETIMEDOUT;
}

int mmc_switch(struct mmc *mmc, uint8_t set, uint8_t index, uint8_t value)
{
    return __mmc_switch(mmc, set, index, value, true);
}

#if 0
static int mmc_set_card_speed(struct mmc *mmc, enum bus_mode mode,
                  bool hsdowngrade)
{
    int err;
    int speed_bits;

    ALLOC_CACHE_ALIGN_BUFFER(uint8_t, test_csd, MMC_MAX_BLOCK_LEN);

    switch (mode) {
    case MMC_HS:
    case MMC_HS_52:
    case MMC_DDR_52:
        speed_bits = EXT_CSD_TIMING_HS;
        break;
    case MMC_LEGACY:
        speed_bits = EXT_CSD_TIMING_LEGACY;
        break;
    default:
        return -EINVAL;
    }

    err = __mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING,
               speed_bits, !hsdowngrade);
    if (err)
        return err;

#if 0
    /*
     * In case the eMMC is in HS200/HS400 mode and we are downgrading
     * to HS mode, the card clock are still running much faster than
     * the supported HS mode clock, so we can not reliably read out
     * Extended CSD. Reconfigure the controller to run at HS mode.
     */
    if (hsdowngrade) {
        mmc_select_mode(mmc, MMC_HS);
        mmc_set_clock(mmc, mmc_mode2freq(mmc, MMC_HS), false);
    }
#endif

    if ((mode == MMC_HS) || (mode == MMC_HS_52)) {
        /* Now check to see that it worked */
        err = mmc_send_ext_csd(mmc, test_csd);
        if (err)
            return err;

        /* No high-speed support */
        if (!test_csd[EXT_CSD_HS_TIMING])
            return -ENOTSUPP;
    }

    return 0;
}

static int mmc_get_capabilities(struct mmc *mmc)
{
    uint8_t *ext_csd = mmc->ext_csd;
    char cardtype;

    mmc->card_caps = MMC_MODE_1BIT | MMC_CAP(MMC_LEGACY);

    // high-speedをサポートしているのはバージョン4だけ
    if (mmc->version < MMC_VERSION_4)
        return 0;

    if (!ext_csd) {
        error("No ext_csd found!"); /* this should enver happen */
        return -ENOTSUPP;
    }

    mmc->card_caps |= MMC_MODE_4BIT | MMC_MODE_8BIT;

    cardtype = ext_csd[EXT_CSD_CARD_TYPE];
    mmc->cardtype = cardtype;

#if 0
    if (cardtype & (EXT_CSD_CARD_TYPE_HS200_1_2V |
            EXT_CSD_CARD_TYPE_HS200_1_8V)) {
        mmc->card_caps |= MMC_MODE_HS200;
    }
#endif
#if 0
    if (cardtype & (EXT_CSD_CARD_TYPE_HS400_1_2V |
            EXT_CSD_CARD_TYPE_HS400_1_8V)) {
        mmc->card_caps |= MMC_MODE_HS400;
    }
#endif
    if (cardtype & EXT_CSD_CARD_TYPE_52) {
        if (cardtype & EXT_CSD_CARD_TYPE_DDR_52)
            mmc->card_caps |= MMC_MODE_DDR_52MHz;
        mmc->card_caps |= MMC_MODE_HS_52MHz;
    }
    if (cardtype & EXT_CSD_CARD_TYPE_26)
        mmc->card_caps |= MMC_MODE_HS;

#if 0
    if (ext_csd[EXT_CSD_STROBE_SUPPORT] &&
        (mmc->card_caps & MMC_MODE_HS400)) {
        mmc->card_caps |= MMC_MODE_HS400_ES;
    }
#endif

    return 0;
}
#endif

#if 0
static int mmc_set_capacity(struct mmc *mmc, int part_num)
{
    switch (part_num) {
    case 0:
        mmc->capacity = mmc->capacity_user;
        break;
    case 1:
    case 2:
        mmc->capacity = mmc->capacity_boot;
        break;
    case 3:
        mmc->capacity = mmc->capacity_rpmb;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        mmc->capacity = mmc->capacity_gp[part_num - 4];
        break;
    default:
        return -1;
    }

    return 0;
}
#endif

// スイッチ機能のチェック
static int sd_switch(struct mmc *mmc, int mode, int group, uint8_t value, uint8_t *resp)
{
    struct mmc_cmd cmd;
    struct mmc_data data;

    /* Switch the frequency */
    cmd.cmdidx = SD_CMD_SWITCH_FUNC;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = (mode << 31) | 0xffffff;
    cmd.cmdarg &= ~(0xf << (group * 4));
    cmd.cmdarg |= value << (group * 4);

    data.dest = (char *)resp;
    data.blocksize = 64;
    data.blocks = 1;
    data.flags = MMC_DATA_READ;

    return mmc_send_cmd(mmc, &cmd, &data);
}

static int sd_get_capabilities(struct mmc *mmc)
{
    int err;
    struct mmc_cmd cmd;
    // FIXME: 以下の2つのuint32_tはもともとは　__be32 だった。bigendianが必要かチェック
    ALLOC_CACHE_ALIGN_BUFFER(uint32_t, scr, 2);
    ALLOC_CACHE_ALIGN_BUFFER(uint32_t, switch_status, 16);
    struct mmc_data data;
    int timeout;

    mmc->card_caps = MMC_MODE_1BIT | MMC_CAP(MMC_LEGACY);

    // SCRを読み込んでこのカードが高速をサポートしているかチェックする
    cmd.cmdidx = MMC_CMD_APP_CMD;       // 55
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = mmc->rca << 16;

    err = mmc_send_cmd(mmc, &cmd, NULL);

    if (err)
        return err;

    cmd.cmdidx = SD_CMD_APP_SEND_SCR;   // 51
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = 0;

    data.dest = (char *)scr;
    data.blocksize = 8;
    data.blocks = 1;
    data.flags = MMC_DATA_READ;

    err = mmc_send_cmd_retry(mmc, &cmd, &data, 3);

    if (err)
        return err;

    mmc->scr[0] = byteswap32(scr[0]);
    mmc->scr[1] = byteswap32(scr[1]);

    switch ((mmc->scr[0] >> 24) & 0xf) {
    case 0:
        mmc->version = SD_VERSION_1_0;
        break;
    case 1:
        mmc->version = SD_VERSION_1_10;
        break;
    case 2:
        mmc->version = SD_VERSION_2;
        if ((mmc->scr[0] >> 15) & 0x1)
            mmc->version = SD_VERSION_3;
        break;
    default:
        mmc->version = SD_VERSION_1_0;
        break;
    }

    if (mmc->scr[0] & SD_DATA_4BIT)
        mmc->card_caps |= MMC_MODE_4BIT;

    // Version 1.0 はスイッチ機能をサポートしていない
    if (mmc->version == SD_VERSION_1_0)
        return 0;

    timeout = 4;
    while (timeout--) {
        err = sd_switch(mmc, SD_SWITCH_CHECK, 0, 1,
                (uint8_t *)switch_status);

        if (err)
            return err;

        // 高速機能はビジー。再試行
        if (!(byteswap32(switch_status[7]) & SD_HIGHSPEED_BUSY))
            break;
    }

    // 高速をサポートしていなかったらリターン
    if (byteswap32(switch_status[3]) & SD_HIGHSPEED_SUPPORTED)
        mmc->card_caps |= MMC_CAP(SD_HS);

    //print_csd_info(mmc);

    // バージョン3未満はUHSモードをサポートしていない
    if (mmc->version < SD_VERSION_3)
        return 0;

    uint32_t sd3_bus_mode = byteswap32(switch_status[3]) >> 16 & 0x1f;
    if (sd3_bus_mode & SD_MODE_UHS_SDR104)
        mmc->card_caps |= MMC_CAP(UHS_SDR104);
    if (sd3_bus_mode & SD_MODE_UHS_SDR50)
        mmc->card_caps |= MMC_CAP(UHS_SDR50);
    if (sd3_bus_mode & SD_MODE_UHS_SDR25)
        mmc->card_caps |= MMC_CAP(UHS_SDR25);
    if (sd3_bus_mode & SD_MODE_UHS_SDR12)
        mmc->card_caps |= MMC_CAP(UHS_SDR12);
    if (sd3_bus_mode & SD_MODE_UHS_DDR50)
        mmc->card_caps |= MMC_CAP(UHS_DDR50);

    return 0;
}

static int sd_set_card_speed(struct mmc *mmc, enum bus_mode mode)
{
    int err;

    ALLOC_CACHE_ALIGN_BUFFER(unsigned int, switch_status, 16);
    int speed;

    /* SD version 1.00 and 1.01 does not support CMD 6 */
    if (mmc->version == SD_VERSION_1_0)
        return 0;

    switch (mode) {
    case MMC_LEGACY:
        speed = UHS_SDR12_BUS_SPEED;
        break;
    case SD_HS:
        speed = HIGH_SPEED_BUS_SPEED;
        break;
    case UHS_SDR12:
        speed = UHS_SDR12_BUS_SPEED;
        break;
    case UHS_SDR25:
        speed = UHS_SDR25_BUS_SPEED;
        break;
    case UHS_SDR50:
        speed = UHS_SDR50_BUS_SPEED;
        break;
    case UHS_DDR50:
        speed = UHS_DDR50_BUS_SPEED;
        break;
    case UHS_SDR104:
        speed = UHS_SDR104_BUS_SPEED;
        break;
    default:
        return -EINVAL;
    }

    err = sd_switch(mmc, SD_SWITCH_SWITCH, 0, speed, (uint8_t *)switch_status);
    if (err)
        return err;

    if (((byteswap32(switch_status[4]) >> 24) & 0xF) != speed)
        return -ENOTSUPP;

    return 0;
}

static int sd_select_bus_width(struct mmc *mmc, int w)
{
    int err;
    struct mmc_cmd cmd;

    if ((w != 4) && (w != 1))
        return -EINVAL;

    cmd.cmdidx = MMC_CMD_APP_CMD;           // 55
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = mmc->rca << 16;

    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err)
        return err;

    cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;  // 6
    cmd.resp_type = MMC_RSP_R1;
    if (w == 4)
        cmd.cmdarg = 2;
    else if (w == 1)
        cmd.cmdarg = 0;
    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err)
        return err;

    return 0;
}

#define SZ_16K				0x00004000
#define SZ_32K				0x00008000
#define SZ_64K				0x00010000
#define SZ_128K				0x00020000
#define SZ_256K				0x00040000
#define SZ_512K				0x00080000

#define SZ_1M				0x00100000
#define SZ_2M				0x00200000
#define SZ_4M				0x00400000
#define SZ_8M				0x00800000
#define SZ_16M				0x01000000
#define SZ_32M				0x02000000
#define SZ_64M				0x04000000

static int sd_read_ssr(struct mmc *mmc)
{
    static const unsigned int sd_au_size[] = {
        0,              SZ_16K / 512,           SZ_32K / 512,
        SZ_64K / 512,   SZ_128K / 512,          SZ_256K / 512,
        SZ_512K / 512,  SZ_1M / 512,            SZ_2M / 512,
        SZ_4M / 512,    SZ_8M / 512,            (SZ_8M + SZ_4M) / 512,
        SZ_16M / 512,   (SZ_16M + SZ_8M) / 512, SZ_32M / 512,
        SZ_64M / 512,
    };
    int err, i;
    struct mmc_cmd cmd;
    ALLOC_CACHE_ALIGN_BUFFER(unsigned int, ssr, 16);
    struct mmc_data data;
    unsigned int au, eo, et, es;

    cmd.cmdidx = MMC_CMD_APP_CMD;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = mmc->rca << 16;

    err = mmc_send_cmd_quirks(mmc, &cmd, NULL, MMC_QUIRK_RETRY_APP_CMD, 4);
    if (err)
        return err;

    cmd.cmdidx = SD_CMD_APP_SD_STATUS;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = 0;

    data.dest = (char *)ssr;
    data.blocksize = 64;
    data.blocks = 1;
    data.flags = MMC_DATA_READ;

    err = mmc_send_cmd_retry(mmc, &cmd, &data, 3);
    if (err)
        return err;

    for (i = 0; i < 16; i++)
        ssr[i] = byteswap32(ssr[i]);

    au = (ssr[2] >> 12) & 0xF;
    if ((au <= 9) || (mmc->version == SD_VERSION_3)) {
        mmc->ssr.au = sd_au_size[au];
        es = (ssr[3] >> 24) & 0xFF;
        es |= (ssr[2] & 0xFF) << 8;
        et = (ssr[3] >> 18) & 0x3F;
        if (es && et) {
            eo = (ssr[3] >> 16) & 0x3;
            mmc->ssr.erase_timeout = (et * 1000) / es;
            mmc->ssr.erase_offset = eo * 1000;
        }
    } else {
        debug("Invalid Allocation Unit Size.");
    }

    return 0;
}

/* frequency bases */
/* divided by 10 to be nice to platforms without floating point */
static const int fbase[] = {
    10000,
    100000,
    1000000,
    10000000,
};

/* Multiplier values for TRAN_SPEED.  Multiplied by 10 to be nice
 * to platforms without floating point.
 */
static const uint8_t multipliers[] = {
    0,    /* reserved */
    10,
    12,
    13,
    15,
    20,
    25,
    30,
    35,
    40,
    45,
    50,
    55,
    60,
    70,
    80,
};

static inline int bus_width(unsigned int cap)
{
    if (cap == MMC_MODE_8BIT)
        return 8;
    if (cap == MMC_MODE_4BIT)
        return 4;
    if (cap == MMC_MODE_1BIT)
        return 1;
    warn("invalid bus witdh capability 0x%x", cap);
    return 0;
}

int mmc_set_clock(struct mmc *mmc, unsigned int clock, bool disable)
{
    if (!disable) {
        if (clock > mmc->cfg->f_max)
            clock = mmc->cfg->f_max;

        if (clock < mmc->cfg->f_min)
            clock = mmc->cfg->f_min;
    }

    mmc->clock = clock;
    mmc->clk_disable = disable;

    debug("clock is %s (%dHz)", disable ? "disabled" : "enabled", clock);

    return mmc_set_ios(mmc);
}

static int mmc_set_bus_width(struct mmc *mmc, unsigned int width)
{
    mmc->bus_width = width;

    return mmc_set_ios(mmc);
}

/*
 * helper function to display the capabilities in a human
 * friendly manner. The capabilities include bus width and
 * supported modes.
 */
void mmc_dump_capabilities(const char *text, unsigned int caps)
{
    enum bus_mode mode;

    debug("%s: widths [", text);
    if (caps & MMC_MODE_8BIT)
        debug("8, ");
    if (caps & MMC_MODE_4BIT)
        debug("4, ");
    if (caps & MMC_MODE_1BIT)
        debug("1, ");
    debug("\b\b] modes [");
    for (mode = MMC_LEGACY; mode < MMC_MODES_END; mode++)
        if (MMC_CAP(mode) & caps)
            debug("%s, ", mmc_mode_name(mode));
    debug("\b\b]");
}

struct mode_width_tuning {
    enum bus_mode mode;
    unsigned int widths;
    unsigned int tuning;
};

static inline int mmc_set_signal_voltage(struct mmc *mmc, unsigned int signal_voltage)
{
    // 3.3v固定で変更できない
    return 0;
}

static const struct mode_width_tuning sd_modes_by_pref[] = {
    {
        .mode = UHS_SDR104,
        .widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
        .tuning = MMC_CMD_SEND_TUNING_BLOCK
    },
    {
        .mode = UHS_SDR50,
        .widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
    },
    {
        .mode = UHS_DDR50,
        .widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
    },
    {
        .mode = UHS_SDR25,
        .widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
    },
    {
        .mode = SD_HS,
        .widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
    },
    {
        .mode = UHS_SDR12,
        .widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
    },
    {
        .mode = MMC_LEGACY,
        .widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
    }
};

#define for_each_sd_mode_by_pref(caps, mwt) \
    for (mwt = sd_modes_by_pref;\
         mwt < sd_modes_by_pref + ARRAY_SIZE(sd_modes_by_pref);\
         mwt++) \
        if (caps & MMC_CAP(mwt->mode))

static int sd_select_mode_and_width(struct mmc *mmc, unsigned int card_caps)
{
    int err;
    unsigned int widths[] = {MMC_MODE_4BIT, MMC_MODE_1BIT};
    const struct mode_width_tuning *mwt;
    bool uhs_en = false;
    unsigned int caps;

#if 0
    mmc_dump_capabilities("sd card", card_caps);
    mmc_dump_capabilities("host", mmc->host_caps);
#endif

    // カードの機能をホストができることに制限する
    caps = card_caps & mmc->host_caps;

    if (!uhs_en)
        caps &= ~UHS_CAPS;

    for_each_sd_mode_by_pref(caps, mwt) {
        unsigned int *w;

        for (w = widths; w < widths + ARRAY_SIZE(widths); w++) {
            if (*w & caps & mwt->widths) {
                debug("trying mode %s width %d (at %d MHz)",
                     mmc_mode_name(mwt->mode),
                     bus_width(*w),
                     mmc_mode2freq(mmc, mwt->mode) / 1000000);

                /* configure the bus width (card + host) */
                err = sd_select_bus_width(mmc, bus_width(*w));
                if (err)
                    goto error;
                mmc_set_bus_width(mmc, bus_width(*w));

                /* configure the bus mode (card) */
                err = sd_set_card_speed(mmc, mwt->mode);
                if (err)
                    goto error;

                /* configure the bus mode (host) */
                mmc_select_mode(mmc, mwt->mode);
                mmc_set_clock(mmc, mmc->tran_speed,
                        MMC_CLK_ENABLE);

                /* execute tuning if needed */
                if (mwt->tuning) {
                    err = mmc_execute_tuning(mmc, mwt->tuning);
                    if (err) {
                        debug("tuning failed");
                        goto error;
                    }
                }
                err = sd_read_ssr(mmc);
                if (err)
                    warn("unable to read ssr");
                if (!err)
                    return 0;

error:
                /* revert to a safer bus speed */
                mmc_select_mode(mmc, MMC_LEGACY);
                mmc_set_clock(mmc, mmc->tran_speed,
                        MMC_CLK_ENABLE);
            }
        }
    }

    error("unable to select a mode");
    return -ENOTSUPP;
}

#if 0
/*
 * read the compare the part of ext csd that is constant.
 * This can be used to check that the transfer is working
 * as expected.
 */
static int mmc_read_and_compare_ext_csd(struct mmc *mmc)
{
    int err;
    const uint8_t *ext_csd = mmc->ext_csd;
    ALLOC_CACHE_ALIGN_BUFFER(uint8_t, test_csd, MMC_MAX_BLOCK_LEN);

    if (mmc->version < MMC_VERSION_4)
        return 0;

    err = mmc_send_ext_csd(mmc, test_csd);
    if (err)
        return err;

    /* Only compare read only fields */
    if (ext_csd[EXT_CSD_PARTITIONING_SUPPORT]
        == test_csd[EXT_CSD_PARTITIONING_SUPPORT] &&
        ext_csd[EXT_CSD_HC_WP_GRP_SIZE]
        == test_csd[EXT_CSD_HC_WP_GRP_SIZE] &&
        ext_csd[EXT_CSD_REV]
        == test_csd[EXT_CSD_REV] &&
        ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]
        == test_csd[EXT_CSD_HC_ERASE_GRP_SIZE] &&
        memcmp(&ext_csd[EXT_CSD_SEC_CNT],
           &test_csd[EXT_CSD_SEC_CNT], 4) == 0)
        return 0;

    return -EBADMSG;
}
#endif

static inline int mmc_set_lowest_voltage(struct mmc *mmc, enum bus_mode mode,
                     uint32_t allowed_mask)
{
    return 0;
}

#if 0
static const struct mode_width_tuning mmc_modes_by_pref[] = {
    {
        .mode = MMC_HS_400_ES,
        .widths = MMC_MODE_8BIT,
    },
    {
        .mode = MMC_HS_400,
        .widths = MMC_MODE_8BIT,
        .tuning = MMC_CMD_SEND_TUNING_BLOCK_HS200
    },
    {
        .mode = MMC_HS_200,
        .widths = MMC_MODE_8BIT | MMC_MODE_4BIT,
        .tuning = MMC_CMD_SEND_TUNING_BLOCK_HS200
    },
    {
        .mode = MMC_DDR_52,
        .widths = MMC_MODE_8BIT | MMC_MODE_4BIT,
    },
    {
        .mode = MMC_HS_52,
        .widths = MMC_MODE_8BIT | MMC_MODE_4BIT | MMC_MODE_1BIT,
    },
    {
        .mode = MMC_HS,
        .widths = MMC_MODE_8BIT | MMC_MODE_4BIT | MMC_MODE_1BIT,
    },
    {
        .mode = MMC_LEGACY,
        .widths = MMC_MODE_8BIT | MMC_MODE_4BIT | MMC_MODE_1BIT,
    }
};

#define for_each_mmc_mode_by_pref(caps, mwt) \
    for (mwt = mmc_modes_by_pref;\
        mwt < mmc_modes_by_pref + ARRAY_SIZE(mmc_modes_by_pref);\
        mwt++) \
        if (caps & MMC_CAP(mwt->mode))


static const struct ext_csd_bus_width {
    unsigned int cap;
    bool is_ddr;
    unsigned int ext_csd_bits;
} ext_csd_bus_width[] = {
    {MMC_MODE_8BIT, true, EXT_CSD_DDR_BUS_WIDTH_8},
    {MMC_MODE_4BIT, true, EXT_CSD_DDR_BUS_WIDTH_4},
    {MMC_MODE_8BIT, false, EXT_CSD_BUS_WIDTH_8},
    {MMC_MODE_4BIT, false, EXT_CSD_BUS_WIDTH_4},
    {MMC_MODE_1BIT, false, EXT_CSD_BUS_WIDTH_1},
};

static int mmc_select_hs400(struct mmc *mmc)
{
    return -ENOTSUPP;
}

static int mmc_select_hs400es(struct mmc *mmc)
{
    return -ENOTSUPP;
}

#define for_each_supported_width(caps, ddr, ecbv) \
    for (ecbv = ext_csd_bus_width;\
        ecbv < ext_csd_bus_width + ARRAY_SIZE(ext_csd_bus_width);\
        ecbv++) \
        if ((ddr == ecbv->is_ddr) && (caps & ecbv->cap))

static int mmc_select_mode_and_width(struct mmc *mmc, unsigned int card_caps)
{
    int err = 0;
    const struct mode_width_tuning *mwt;
    const struct ext_csd_bus_width *ecbw;

#if 0
    mmc_dump_capabilities("mmc", card_caps);
    mmc_dump_capabilities("host", mmc->host_caps);
#endif

    /* Restrict card's capabilities by what the host can do */
    //card_caps &= mmc->host_caps;

    /* Only version 4 of MMC supports wider bus widths */
    if (mmc->version < MMC_VERSION_4)
        return 0;

    if (!mmc->ext_csd) {
        debug("No ext_csd found!"); /* this should enver happen */
        return -ENOTSUPP;
    }

    /*
     * In case the eMMC is in HS200/HS400 mode, downgrade to HS mode
     * before doing anything else, since a transition from either of
     * the HS200/HS400 mode directly to legacy mode is not supported.
     */
    if (mmc->selected_mode == MMC_HS_200 ||
        mmc->selected_mode == MMC_HS_400 ||
        mmc->selected_mode == MMC_HS_400_ES)
        mmc_set_card_speed(mmc, MMC_HS, true);
    else
        mmc_set_clock(mmc, mmc->legacy_speed, MMC_CLK_ENABLE);

    for_each_mmc_mode_by_pref(card_caps, mwt) {
        for_each_supported_width(card_caps & mwt->widths,
                     mmc_is_mode_ddr(mwt->mode), ecbw) {
            enum mmc_voltage old_voltage;
            debug("trying mode %s width %d (at %d MHz)",
                 mmc_mode_name(mwt->mode),
                 bus_width(ecbw->cap),
                 mmc_mode2freq(mmc, mwt->mode) / 1000000);
            old_voltage = mmc->signal_voltage;
            err = mmc_set_lowest_voltage(mmc, mwt->mode,
                             MMC_ALL_SIGNAL_VOLTAGE);
            if (err)
                continue;

            /* configure the bus width (card + host) */
            err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
                    EXT_CSD_BUS_WIDTH,
                    ecbw->ext_csd_bits & ~EXT_CSD_DDR_FLAG);
            if (err)
                goto error;
            mmc_set_bus_width(mmc, bus_width(ecbw->cap));

            if (mwt->mode == MMC_HS_400) {
                err = mmc_select_hs400(mmc);
                if (err) {
                    cprintf("Select HS400 failed %d\n", err);
                    goto error;
                }
            } else if (mwt->mode == MMC_HS_400_ES) {
                err = mmc_select_hs400es(mmc);
                if (err) {
                    cprintf("Select HS400ES failed %d\n",
                           err);
                    goto error;
                }
            } else {
                /* configure the bus speed (card) */
                err = mmc_set_card_speed(mmc, mwt->mode, false);
                if (err)
                    goto error;

                /*
                 * configure the bus width AND the ddr mode
                 * (card). The host side will be taken care
                 * of in the next step
                 */
                if (ecbw->ext_csd_bits & EXT_CSD_DDR_FLAG) {
                    err = mmc_switch(mmc,
                             EXT_CSD_CMD_SET_NORMAL,
                             EXT_CSD_BUS_WIDTH,
                             ecbw->ext_csd_bits);
                    if (err)
                        goto error;
                }

                /* configure the bus mode (host) */
                mmc_select_mode(mmc, mwt->mode);
                mmc_set_clock(mmc, mmc->tran_speed,
                          MMC_CLK_ENABLE);

                /* execute tuning if needed */
                if (mwt->tuning) {
                    err = mmc_execute_tuning(mmc,
                                 mwt->tuning);
                    if (err) {
                        debug("tuning failed : %d", err);
                        goto error;
                    }
                }
            }

            /* do a transfer to check the configuration */
            err = mmc_read_and_compare_ext_csd(mmc);
            if (!err)
                return 0;
error:
            mmc_set_signal_voltage(mmc, old_voltage);
            /* if an error occurred, revert to a safer bus mode */
            mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
                   EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_1);
            mmc_select_mode(mmc, MMC_LEGACY);
            mmc_set_clock(mmc, mmc->legacy_speed, MMC_CLK_ENABLE);
            mmc_set_bus_width(mmc, 1);
        }
    }

    error("unable to select a mode : %d", err);

    return -ENOTSUPP;
}

static int mmc_startup_v4(struct mmc *mmc)
{
    int err, i;
    u64 capacity;
    bool has_parts = false;
    bool part_completed;
    static const u32 mmc_versions[] = {
        MMC_VERSION_4,
        MMC_VERSION_4_1,
        MMC_VERSION_4_2,
        MMC_VERSION_4_3,
        MMC_VERSION_4_4,
        MMC_VERSION_4_41,
        MMC_VERSION_4_5,
        MMC_VERSION_5_0,
        MMC_VERSION_5_1
    };

    ALLOC_CACHE_ALIGN_BUFFER(uint8_t, ext_csd, MMC_MAX_BLOCK_LEN);

    if (IS_SD(mmc) || (mmc->version < MMC_VERSION_4))
        return 0;

    /* check  ext_csd version and capacity */
    err = mmc_send_ext_csd(mmc, ext_csd);
    if (err)
        goto error;

    /* store the ext csd for future reference */
    if (!mmc->ext_csd)
        mmc->ext_csd = malloc(MMC_MAX_BLOCK_LEN);
    if (!mmc->ext_csd)
        return -ENOMEM;
    memcpy(mmc->ext_csd, ext_csd, MMC_MAX_BLOCK_LEN);

    if (ext_csd[EXT_CSD_REV] >= ARRAY_SIZE(mmc_versions))
        return -EINVAL;

    mmc->version = mmc_versions[ext_csd[EXT_CSD_REV]];

    if (mmc->version >= MMC_VERSION_4_2) {
        /*
         * According to the JEDEC Standard, the value of
         * ext_csd's capacity is valid if the value is more
         * than 2GB
         */
        capacity = ext_csd[EXT_CSD_SEC_CNT] << 0
                | ext_csd[EXT_CSD_SEC_CNT + 1] << 8
                | ext_csd[EXT_CSD_SEC_CNT + 2] << 16
                | ext_csd[EXT_CSD_SEC_CNT + 3] << 24;
        capacity *= MMC_MAX_BLOCK_LEN;
        if ((capacity >> 20) > 2 * 1024)
            mmc->capacity_user = capacity;
    }

#if 0
    if (mmc->version >= MMC_VERSION_4_5)
        mmc->gen_cmd6_time = ext_csd[EXT_CSD_GENERIC_CMD6_TIME];

    /* The partition data may be non-zero but it is only
     * effective if PARTITION_SETTING_COMPLETED is set in
     * EXT_CSD, so ignore any data if this bit is not set,
     * except for enabling the high-capacity group size
     * definition (see below).
     */

    part_completed = !!(ext_csd[EXT_CSD_PARTITION_SETTING] &
                EXT_CSD_PARTITION_SETTING_COMPLETED);

    mmc->part_switch_time = ext_csd[EXT_CSD_PART_SWITCH_TIME];
    /* Some eMMC set the value too low so set a minimum */
    if (mmc->part_switch_time < MMC_MIN_PART_SWITCH_TIME && mmc->part_switch_time)
        mmc->part_switch_time = MMC_MIN_PART_SWITCH_TIME;

    /* store the partition info of emmc */
    mmc->part_support = ext_csd[EXT_CSD_PARTITIONING_SUPPORT];
    if ((ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & PART_SUPPORT) ||
        ext_csd[EXT_CSD_BOOT_MULT])
        mmc->part_config = ext_csd[EXT_CSD_PART_CONF];
    if (part_completed &&
        (ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & ENHNCD_SUPPORT))
        mmc->part_attr = ext_csd[EXT_CSD_PARTITIONS_ATTRIBUTE];
#endif
    mmc->capacity_boot = ext_csd[EXT_CSD_BOOT_MULT] << 17;

    mmc->capacity_rpmb = ext_csd[EXT_CSD_RPMB_MULT] << 17;

    for (i = 0; i < 4; i++) {
        int idx = EXT_CSD_GP_SIZE_MULT + i * 3;
        unsigned int mult = (ext_csd[idx + 2] << 16) +
            (ext_csd[idx + 1] << 8) + ext_csd[idx];
        if (mult)
            has_parts = true;
        if (!part_completed)
            continue;
        mmc->capacity_gp[i] = mult;
        mmc->capacity_gp[i] *=
            ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
        mmc->capacity_gp[i] *= ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
        mmc->capacity_gp[i] <<= 19;
    }
#if 0
    if (part_completed) {
        mmc->enh_user_size =
            (ext_csd[EXT_CSD_ENH_SIZE_MULT + 2] << 16) +
            (ext_csd[EXT_CSD_ENH_SIZE_MULT + 1] << 8) +
            ext_csd[EXT_CSD_ENH_SIZE_MULT];
        mmc->enh_user_size *= ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
        mmc->enh_user_size *= ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
        mmc->enh_user_size <<= 19;
        mmc->enh_user_start =
            (ext_csd[EXT_CSD_ENH_START_ADDR + 3] << 24) +
            (ext_csd[EXT_CSD_ENH_START_ADDR + 2] << 16) +
            (ext_csd[EXT_CSD_ENH_START_ADDR + 1] << 8) +
            ext_csd[EXT_CSD_ENH_START_ADDR];
        if (mmc->high_capacity)
            mmc->enh_user_start <<= 9;
    }

    /*
     * Host needs to enable ERASE_GRP_DEF bit if device is
     * partitioned. This bit will be lost every time after a reset
     * or power off. This will affect erase size.
     */
    if (part_completed)
        has_parts = true;
    if ((ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & PART_SUPPORT) &&
        (ext_csd[EXT_CSD_PARTITIONS_ATTRIBUTE] & PART_ENH_ATTRIB))
        has_parts = true;
    if (has_parts) {
        err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
                 EXT_CSD_ERASE_GROUP_DEF, 1);

        if (err)
            goto error;

        ext_csd[EXT_CSD_ERASE_GROUP_DEF] = 1;
    }

    if (ext_csd[EXT_CSD_ERASE_GROUP_DEF] & 0x01) {
        /* Read out group size from ext_csd */
        mmc->erase_grp_size =
            ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] * 1024;
        /*
         * if high capacity and partition setting completed
         * SEC_COUNT is valid even if it is smaller than 2 GiB
         * JEDEC Standard JESD84-B45, 6.2.4
         */
        if (mmc->high_capacity && part_completed) {
            capacity = (ext_csd[EXT_CSD_SEC_CNT]) |
                (ext_csd[EXT_CSD_SEC_CNT + 1] << 8) |
                (ext_csd[EXT_CSD_SEC_CNT + 2] << 16) |
                (ext_csd[EXT_CSD_SEC_CNT + 3] << 24);
            capacity *= MMC_MAX_BLOCK_LEN;
            mmc->capacity_user = capacity;
        }
    }
    else {
        /* Calculate the group size from the csd value. */
        int erase_gsz, erase_gmul;

        erase_gsz = (mmc->csd[2] & 0x00007c00) >> 10;
        erase_gmul = (mmc->csd[2] & 0x000003e0) >> 5;
        mmc->erase_grp_size = (erase_gsz + 1)
            * (erase_gmul + 1);
    }
    mmc->hc_wp_grp_size = 1024
        * ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]
        * ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

    mmc->wr_rel_set = ext_csd[EXT_CSD_WR_REL_SET];

    mmc->can_trim =
        !!(ext_csd[EXT_CSD_SEC_FEATURE] & EXT_CSD_SEC_FEATURE_TRIM_EN);
#endif

    return 0;
error:
    if (mmc->ext_csd) {
        free(mmc->ext_csd);
        mmc->ext_csd = NULL;
    }
    return err;
}
#endif

// (4) mmcを起動する
static int mmc_startup(struct mmc *mmc)
{
    int err, i;
    unsigned int mult, freq;
    u64 cmult, csize;
    struct mmc_cmd cmd;

    /* 1. カードをIdentifyモードにする */
    cmd.cmdidx = MMC_CMD_ALL_SEND_CID;      // 2
    cmd.resp_type = MMC_RSP_R2;
    cmd.cmdarg = 0;

    err = mmc_send_cmd_quirks(mmc, &cmd, NULL, MMC_QUIRK_RETRY_SEND_CID, 4);
    if (err)
        return err;

    // 2. CIDを設定する
    memcpy(mmc->cid, cmd.response, 16);   // ここでストール

    /*
     * 3. SDカードでは相対アドレスを取得する.
     * これでカードはStandby状態になる
     */
    cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR; // 3
    cmd.cmdarg = mmc->rca << 16;
    cmd.resp_type = MMC_RSP_R6;

    err = mmc_send_cmd(mmc, &cmd, NULL);

    if (err)
        return err;

    if (IS_SD(mmc))
        mmc->rca = (cmd.response[0] >> 16) & 0xffff;

    // 4. カード固有データを取得してCSDを取得する (CMD9)
    cmd.cmdidx = MMC_CMD_SEND_CSD;
    cmd.resp_type = MMC_RSP_R2;
    cmd.cmdarg = mmc->rca << 16;

    err = mmc_send_cmd(mmc, &cmd, NULL);

    if (err)
        return err;

    mmc->csd[0] = cmd.response[0];
    mmc->csd[1] = cmd.response[1];
    mmc->csd[2] = cmd.response[2];
    mmc->csd[3] = cmd.response[3];

    // 5. CSDから各種情報をセットする
    if (mmc->version == MMC_VERSION_UNKNOWN) {
        int version = (cmd.response[0] >> 26) & 0xf;

        switch (version) {
        case 0:
            mmc->version = MMC_VERSION_1_2;
            break;
        case 1:
            mmc->version = MMC_VERSION_1_4;
            break;
        case 2:
            mmc->version = MMC_VERSION_2_2;
            break;
        case 3:
            mmc->version = MMC_VERSION_3;
            break;
        case 4:
            mmc->version = MMC_VERSION_4;
            break;
        default:
            mmc->version = MMC_VERSION_1_2;
            break;
        }
    }

    /* divide frequency by 10, since the mults are 10x bigger */
    freq = fbase[(cmd.response[0] & 0x7)];
    mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

    mmc->legacy_speed = freq * mult;
    mmc_select_mode(mmc, MMC_LEGACY);

    mmc->dsr_imp = ((cmd.response[1] >> 12) & 0x1);
    mmc->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);

    if (IS_SD(mmc))
        mmc->write_bl_len = mmc->read_bl_len;
    else
        mmc->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);

    if (mmc->high_capacity) {
        csize = (mmc->csd[1] & 0x3f) << 16
            | (mmc->csd[2] & 0xffff0000) >> 16;
        cmult = 8;
    } else {
        csize = (mmc->csd[1] & 0x3ff) << 2
            | (mmc->csd[2] & 0xc0000000) >> 30;
        cmult = (mmc->csd[2] & 0x00038000) >> 15;
    }

    mmc->capacity_user = (csize + 1) << (cmult + 2);
    mmc->capacity_user *= mmc->read_bl_len;
    mmc->capacity_boot = 0;
    mmc->capacity_rpmb = 0;
    for (i = 0; i < 4; i++)
        mmc->capacity_gp[i] = 0;

    if (mmc->read_bl_len > MMC_MAX_BLOCK_LEN)
        mmc->read_bl_len = MMC_MAX_BLOCK_LEN;

    if (mmc->write_bl_len > MMC_MAX_BLOCK_LEN)
        mmc->write_bl_len = MMC_MAX_BLOCK_LEN;

    //print_csd_info(mmc);

    // 6. CMD4を発行してDSRをセットする : mmc->dsr = 0xffffffffにセットされているので実行されない
    if ((mmc->dsr_imp) && (0xffffffff != mmc->dsr)) {
        cmd.cmdidx = MMC_CMD_SET_DSR;
        cmd.cmdarg = (mmc->dsr & 0xffff) << 16;
        cmd.resp_type = MMC_RSP_NONE;
        if (mmc_send_cmd(mmc, &cmd, NULL))
            warn("MMC: SET_DSR failed");
    }

    // 7. カードを選択して、Transferモードに置く : CMD7
    cmd.cmdidx = MMC_CMD_SELECT_CARD;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = mmc->rca << 16;
    err = mmc_send_cmd(mmc, &cmd, NULL);

    if (err)
        return err;

    // 8. SDではeraseグループは常に1セクタ
    mmc->erase_grp_size = 1;

    err = sd_get_capabilities(mmc);
    if (err)
        return err;
    err = sd_select_mode_and_width(mmc, mmc->card_caps);

    if (err)
        return err;

    mmc->best_mode = mmc->selected_mode;

    /* Fix the block length for DDR mode */
    if (mmc->ddr_mode) {
        mmc->read_bl_len = MMC_MAX_BLOCK_LEN;
        mmc->write_bl_len = MMC_MAX_BLOCK_LEN;
    }

    return 0;
}

// SD バージョン2をサポートしているかチェックする
static int mmc_send_if_cond(struct mmc *mmc)
{
    struct mmc_cmd cmd;
    int err;

    cmd.cmdidx = SD_CMD_SEND_IF_COND;
    /* We set the bit if the host supports voltages between 2.7 and 3.6 V */
    cmd.cmdarg = ((mmc->cfg->voltages & 0xff8000) != 0) << 8 | 0xaa;
    cmd.resp_type = MMC_RSP_R7;

    err = mmc_send_cmd(mmc, &cmd, NULL);

    if (err)
        return err;

    if ((cmd.response[0] & 0xff) != 0xaa)
        return -EOPNOTSUPP;
    else
        mmc->version = SD_VERSION_2;        // バージョン2

    trace("sd version 2 support");
    return 0;
}

static int mmc_power_init(struct mmc *mmc)
{
    // vmmc 3.3vは常時オン
    return 0;
}

/*
 * ホストを初期状態にする:
 * - Vdd (カードに電源供給)をオン
 * - バス幅とクロックを最小値に構成
 */
static void mmc_set_initial_state(struct mmc *mmc)
{
    int err;

    // まず、3.3Vの設定を試みる。失敗したら1.8Vにセットする
    err = mmc_set_signal_voltage(mmc, MMC_SIGNAL_VOLTAGE_330);
    if (err != 0)
        err = mmc_set_signal_voltage(mmc, MMC_SIGNAL_VOLTAGE_180);
    if (err != 0)
        warn("mmc: failed to set signal voltage");

    mmc_select_mode(mmc, MMC_LEGACY);
    mmc_set_bus_width(mmc, 1);
    mmc_set_clock(mmc, 0, MMC_CLK_ENABLE);
}

static int mmc_power_on(struct mmc *mmc)
{
    return 0;
}

static int mmc_power_off(struct mmc *mmc)
{
    mmc_set_clock(mmc, 0, MMC_CLK_DISABLE);
    return 0;
}

static int mmc_power_cycle(struct mmc *mmc)
{
    int ret;

    ret = mmc_power_off(mmc);
    if (ret)
        return ret;

    /*
     * SD spec recommends at least 1ms of delay. Let's wait for 2ms
     * to be on the safer side.
     */
    delayus(2000);
    return mmc_power_on(mmc);
}

int mmc_get_op_cond(struct mmc *mmc, bool quiet)
{
    bool uhs_en = supports_uhs(mmc->card_caps);
    int err;

    if (mmc->has_init)
        return 0;

    // 電源を初期化する
    err = mmc_power_init(mmc);
    if (err)
        return err;

    mmc->quirks = MMC_QUIRK_RETRY_SET_BLOCKLEN |
              MMC_QUIRK_RETRY_SEND_CID |
              MMC_QUIRK_RETRY_APP_CMD;

    // 電源を再投入する
    err = mmc_power_cycle(mmc);
    if (err) {
        /*
         * 電源の再投入がサポートされていない場合、UHSモードを使用すべきでは
         * ない。UHSの初期化中にエラーが発生した際、そこから復旧することが
         * できないからである。
         */
        debug("Unable to do a full power cycle. Disabling the UHS modes for safety");
        uhs_en = false;
        mmc->host_caps &= ~UHS_CAPS;
        err = mmc_power_on(mmc);
    }
    if (err)
        return err;

    mmc->ddr_mode = 0;

retry:
    // ホストを初期状態に置く
    mmc_set_initial_state(mmc);

    // カードをリセット : CMD0
    err = mmc_go_idle(mmc);

    if (err)
        return err;

    // SD バージョン2をサポートしているかチェックする
    err = mmc_send_if_cond(mmc);

    // SDカードの動作条件の取得を試みる
    err = sd_send_op_cond(mmc, uhs_en);
    if (err && uhs_en) {
        uhs_en = false;
        mmc_power_cycle(mmc);
        goto retry;
    }

    // コマンドタイムアウトの場合、MMCカードでないかチェックする
    if (err == -ETIMEDOUT) {
        err = mmc_send_op_cond(mmc);

        if (err) {
            if (!quiet)
                error("Card did not respond to voltage select! : %d", err);
            return -EOPNOTSUPP;
        }
    }

    return err;
}

// (2) 初期化を行う
int mmc_start_init(struct mmc *mmc)
{
    bool no_card;
    int err = 0;

    // すべてのホストは1ビットバス幅に対応しており、レガシータイミングを利用できる
    mmc->host_caps = mmc->cfg->host_caps | MMC_CAP(MMC_LEGACY) |
             MMC_MODE_1BIT;

    // sunxi固有の設定を行う
    sunxi_mmc_probe(mmc);

    // SDカードが挿入されているかチェックする
    no_card = mmc_getcd(mmc) == 0;

    if (no_card) {
        mmc->has_init = 0;
        error("MMC: no card present");
        return -ENOMEDIUM;
    }

    // デバイスを初期化する
    err = mmc_get_op_cond(mmc, false);

    return err;
}

// (3) 初期化を完了させる
static int mmc_complete_init(struct mmc *mmc)
{
    int err = 0;

    if (mmc->op_cond_pending)
        err = mmc_complete_op_cond(mmc);

    if (!err)
        err = mmc_startup(mmc);
    if (err)
        mmc->has_init = 0;
    else
        mmc->has_init = 1;
    return err;
}

// (1) sd_init()から呼び出される : mmc = &sd0
int mmc_initialize(struct mmc *mmc)
{
    int err;

    mmc->user_speed_mode = MMC_MODES_END;
    mmc->dsr_imp = 0;
    mmc->dsr = (unsigned int)-1;

    err = mmc_start_init(mmc);
    if (!err)
        err = mmc_complete_init(mmc);

    info("ok");
    return err;
}


//============= mmc_read 関連 ==============

// startからblkcnt個ブロックを読み込む : CMD17/CMD18
static int mmc_read_blocks(struct mmc *mmc, void *dst, lbaint_t start,
               lbaint_t blkcnt)
{
    struct mmc_cmd cmd;
    struct mmc_data data;

    if (blkcnt > 1)
        cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
    else
        cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;

    // 開始ブロックの指定: HCはブロック番号、それ以外はバイト位置
    if (mmc->high_capacity)
        cmd.cmdarg = start;
    else
        cmd.cmdarg = start * mmc->read_bl_len;

    cmd.resp_type = MMC_RSP_R1;

    data.dest = dst;
    data.blocks = blkcnt;
    data.blocksize = mmc->read_bl_len;
    data.flags = MMC_DATA_READ;

    if (mmc_send_cmd(mmc, &cmd, &data))
        return 0;

    if (blkcnt > 1) {
        if (mmc_send_stop_transmission(mmc, false)) {
            error("mmc fail to send stop cmd");
            return 0;
        }
    }

    return blkcnt;
}

// MBRを読み込む
unsigned long mmc_bread_mbr(struct mmc *mmc, void *dst)
{
    if (mmc_read_blocks(mmc, dst, 0, 2) != 2) {
        error("Failed to read mbr");
        return 0;
    }

    return 2;
}

// ブロック読み込みの公開関数 : startからblkcnt個ブロックを読み込む
//   読み込んだブロク数を返す
unsigned long mmc_bread(struct mmc *mmc, lbaint_t start, lbaint_t blkcnt, void *dst)
{
    lbaint_t cur, blocks_todo = blkcnt;
    unsigned int b_max;

    // 1. 読み込みブロック数が0なら何もしない
    if (blkcnt == 0)
        return 0;

    // 2. 指定のブロック範囲がパーティション内に収まっているかチェックする
    if (!check_block_range(start, blkcnt)) {
        error("range 0x%lx:0x%lx cross pertition", start, blkcnt);
        return 0;
    }

    // 3. ブロック長をセット
    if (mmc_set_blocklen(mmc, mmc->read_bl_len)) {
        error("Failed to set blocklen");
        return 0;
    }

    // 4. 一度に処理できる最大ブロック数を取得
    b_max = mmc_get_b_max(mmc, dst, blkcnt);

     // 5. ブロックを読み込む
    do {
        cur = (blocks_todo > b_max) ? b_max : blocks_todo;
        if (mmc_read_blocks(mmc, dst, start, cur) != cur) {
            debug("%s: Failed to read blocks", __func__);
            return 0;
        }
        blocks_todo -= cur;
        start += cur;
        dst += cur * mmc->read_bl_len;
    } while (blocks_todo > 0);

    return blkcnt;
}

//============ 以下は mmc_write.c の内容 ===================

static unsigned long mmc_erase_t(struct mmc *mmc, unsigned long start, lbaint_t blkcnt, u32 args)
{
    struct mmc_cmd cmd;
    unsigned long end;
    int err, start_cmd, end_cmd;

    if (mmc->high_capacity) {
        end = start + blkcnt - 1;
    } else {
        end = (start + blkcnt - 1) * mmc->write_bl_len;
        start *= mmc->write_bl_len;
    }

    start_cmd = SD_CMD_ERASE_WR_BLK_START;      // CMD32
    end_cmd = SD_CMD_ERASE_WR_BLK_END;          // CMD33

    // 1. 消去する最初のブロックアドレスを指定する
    cmd.cmdidx = start_cmd;
    cmd.cmdarg = start;
    cmd.resp_type = MMC_RSP_R1;

    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err)
        goto err_out;

    // 2. 消去する最後のブロックアドレスを指定する
    cmd.cmdidx = end_cmd;
    cmd.cmdarg = end;

    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err)
        goto err_out;

    // 3. 消去する
    cmd.cmdidx = MMC_CMD_ERASE;                 // CMD38
    cmd.cmdarg = args ? args : MMC_ERASE_ARG;
    cmd.resp_type = MMC_RSP_R1b;

    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err)
        goto err_out;

    return 0;

err_out:
    error("mmc erase failed");
    return err;
}

unsigned long mmc_berase(struct mmc *mmc, lbaint_t start, lbaint_t blkcnt)
{
    int err = 0;
    uint32_t start_rem, blkcnt_rem, erase_args = 0;
    lbaint_t blk = 0, blk_r = 0;
    int timeout_ms = 1000;

    if (!mmc)
        return -1;

    /*
     * 要求された開始ブロック数や合計ブロック数がアラインされて
     * いるかを確認する。 全体の数は捨て、余りだけを気にする。
     */
    err = div_u64_rem(start, mmc->erase_grp_size, &start_rem);
    err = div_u64_rem(blkcnt, mmc->erase_grp_size, &blkcnt_rem);
    if (start_rem || blkcnt_rem) {
        if (mmc->can_trim) {
            /* Trim function applies the erase operation to write
             * blocks instead of erase groups.
             */
            erase_args = MMC_TRIM_ARG;
        } else {
            /* The card ignores all LSB's below the erase group
             * size, rounding down the addess to a erase group
             * boundary.
             */
            info("\nCaution! Your devices Erase group is 0x%x"
                   "The erase range would be change to "
                   "0x%x~0x%x",
                   mmc->erase_grp_size, start & ~(mmc->erase_grp_size - 1),
                   ((start + blkcnt + mmc->erase_grp_size - 1)
                   & ~(mmc->erase_grp_size - 1)) - 1);
        }
    }

    while (blk < blkcnt) {
        if (IS_SD(mmc) && mmc->ssr.au) {
            blk_r = ((blkcnt - blk) > mmc->ssr.au) ?
                mmc->ssr.au : (blkcnt - blk);
        } else {
            blk_r = ((blkcnt - blk) > mmc->erase_grp_size) ?
                mmc->erase_grp_size : (blkcnt - blk);
        }
        err = mmc_erase_t(mmc, start + blk, blk_r, erase_args);
        if (err)
            break;

        blk += blk_r;

        /* Waiting for the ready status */
        if (mmc_poll_for_busy(mmc, timeout_ms))
            return 0;
    }

    return blk;
}

static unsigned long mmc_write_blocks(struct mmc *mmc, lbaint_t start,
        lbaint_t blkcnt, const void *src)
{
    struct mmc_cmd cmd;
    struct mmc_data data;
    int timeout_ms = 1000;

    if (!check_block_range(start, blkcnt)) {
        error("range 0x%lx:0x%lx cross pertition", start, blkcnt);
        return 0;
    }

    if (blkcnt == 0)
        return 0;
    else if (blkcnt == 1)
        cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;
    else
        cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;

    if (mmc->high_capacity)
        cmd.cmdarg = start;
    else
        cmd.cmdarg = start * mmc->write_bl_len;

    // HCの場合はブロック番号で、そうでない場合はデータアドレスでブロックを指定
    if (mmc->high_capacity)
        cmd.cmdarg = start;
    else
        cmd.cmdarg = start * mmc->write_bl_len;

    cmd.resp_type = MMC_RSP_R1;

    data.src = src;
    data.blocks = blkcnt;
    data.blocksize = mmc->write_bl_len;
    data.flags = MMC_DATA_WRITE;

    if (mmc_send_cmd(mmc, &cmd, &data)) {
        error("mmc write failed");
        return 0;
    }

    // STOP_TRANSMISSIONコマンドを送信
    if (blkcnt > 1) {
        cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
        cmd.cmdarg = 0;
        cmd.resp_type = MMC_RSP_R1b;
        if (mmc_send_cmd(mmc, &cmd, NULL)) {
            error("mmc fail to send stop cmd");
            return 0;
        }
    }

    // ready状態になるのを待つ
    if (mmc_poll_for_busy(mmc, timeout_ms))
        return 0;

    return blkcnt;
}

// srcのデータをdevのstart位置からblkcntブロックを書き込む
unsigned long mmc_bwrite(struct mmc *mmc, lbaint_t start, lbaint_t blkcnt,
         const void *src)
{
    lbaint_t cur, blocks_todo = blkcnt;
    unsigned int b_max = mmc_get_b_max(mmc, NULL, blkcnt);

    if (mmc_set_blocklen(mmc, mmc->write_bl_len))
        return 0;

    do {
        cur = (blocks_todo > b_max) ? b_max : blocks_todo;
        if (mmc_write_blocks(mmc, start, cur, src) != cur)
            return 0;
        blocks_todo -= cur;
        start += cur;
        src += cur * mmc->write_bl_len;
    } while (blocks_todo > 0);

    return blkcnt;
}
