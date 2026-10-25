# xv6のemm関連

- xv6はSD Hostは使用していない
- 割り込みも使用していない

## 公開関数

```c
void emmc_clear_interrupt();
void emmc_intr(struct emmc *self);
int emmc_init(struct emmc *self, void (*sleep_fn)(void *), void *sleep_arg);
size_t emmc_read(struct emmc *self, void *buf, size_t cnt);
size_t emmc_write(struct emmc *self, void *buf, size_t cnt);
uint64_t emmc_seek(struct emmc *self, uint64_t off);
```

## static関数

```c
static int emmc_ensure_data_mode(struct emmc *self);
static int emmc_do_data_command(struct emmc *self, int is_write,
                                uint8_t * buf, size_t buf_size,
                                uint32_t block_no);
static size_t emmc_do_read(struct emmc *self, uint8_t * buf,
                           size_t buf_size, uint32_t block_no);
static size_t emmc_do_write(struct emmc *self, uint8_t * buf,
                            size_t buf_size, uint32_t block_no);

static int emmc_card_init(struct emmc *self);
static int emmc_card_reset(struct emmc *self);

static int emmc_issue_command(struct emmc *self, uint32_t cmd,
                              uint32_t arg, int timeout);
static void emmc_issue_command_int(struct emmc *self, uint32_t reg,
                                   uint32_t arg, int timeout);
```

## 構造体

```c
struct tscr // SD configuration register
{
	uint32_t	scr[2];			// SCRレジスタ値 (64bit)
	uint32_t	sd_bus_widths;	// バス幅 ([51:48]: 4bit)
	int	sd_version;				// 仕様バージョン ([59:56]: 4bit)
};

struct emmc {
    uint64_t ull_offset;

#ifdef USE_SDHOST
    struct bcm2835_host host;
#else
	uint32_t hci_ver;
#endif

	// was: struct emmc_block_dev
	uint32_t device_id[4];

	uint32_t card_supports_sdhc;
	uint32_t card_supports_hs;
	uint32_t card_supports_18v;
	uint32_t card_ocr;
	uint32_t card_rca;
#ifndef USE_SDHOST
	uint32_t last_interrupt;
#endif
	uint32_t last_error;

    struct tscr scr;

	int failed_voltage_switch;

	uint32_t last_cmd_reg;
	uint32_t last_cmd;
	uint32_t last_cmd_success;
	uint32_t last_r0;
	uint32_t last_r1;
	uint32_t last_r2;
	uint32_t last_r3;

	void *buf;
	int blocks_to_transfer;
	size_t block_size;

#ifndef USE_SDHOST
	int card_removal;
	uint32_t base_clock;
#endif
	// static const char *sd_versions[];
	// static const uint32_t sd_commands[];
	// static const uint32_t sd_acommands[];
};
```

## 初期化の流れ

```
forkret(): proc.c
  dev_init(): dev.c
    static struct emmc card; : dev.c
    struct mbr mbr;
    emmc_init(&card, dev_sleep, (void *)&card): emmc.c
    	emmc_card_init(self): emmc.c
    		emmc_power_on();
    		self->hci_ver = (get32(EMMC_SLOTISR_VER) >> 16 ) && 0xff);
   	struct buf b; b.dev = ROOTDEV;
    mbr = devrw(&b); // read MBR of SDcard
```

## MMCのread/write処理

0. static struct emmc card; static struct list_head devque;
1. struct buff b = { .blockno = Num, .flags = Flag, .dev = DevNO};
   blks = b-dev == FAT ? 512 : 4096;
2. list_push_back(&devque, &b->dlink);
3. emmc_seek(&card, bno * SD_BLOCK_SIZE); // bno = b.blockno * blkx / 512;
4. b->flags & B_DIRTY ? emmc_write(&card, b->data, blks) : emmc_read(&card, b->data, blks);

    