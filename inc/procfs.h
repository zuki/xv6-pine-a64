#ifndef INC_PROCFS_H
#define INC_PROCFS_H

// procfsに関する考察

// READ
// 1. readオフセットを追跡する必要がある。ユーザは open() と
// close() の間にファイルを連続して複数回読み込む可能性があるため。
// ユーザは、すべてのコンテンツを読み終え、停止すべきタイミング
//（read() から 0 が返されたとき）を知る必要がある。
// 2. open() と close() の間、カーネルの状態が変化しても、
// read() による読み取り結果は一貫した内容でなければならない。
// 例：/proc/meminfo

// WRITE
// 1. writeオフセットを追跡する必要がある。ユーザは procfs ファイルに
// 何度も書き込みを行う可能性があるため（例: "echo"は引数を個別の
// write() システムコールで書き込む）、ファイルが閉じられた後に
// のみ解析を行いたいからである。
// 2. readとwrteではコンテンツとオフセットが異なる場合がある。

#define MAX_PROCFS_SIZE 256
struct procfs_state {
    char kbuf[MAX_PROCFS_SIZE]; // カーネルコンテンツ、open()時に生成される
    uint ksize;                 // カーネルコンテンツの合計サイズ
    char ubuf[MAX_PROCFS_SIZE]; // ユーザが書き込んだコンテンツ、close()時にパースされる
    uint usize;                 // ユーザが書き込んだ最大オフセット
    uint uoff, koff;            // ユーザのread/write用
};
_Static_assert(sizeof (struct procfs_state) < 4096);

struct file;

// いずれも kernel/file.c で実装
void procfs_init_state(struct procfs_state *st);            // sys_open()からcall
int procfs_gen_content(int major, char *txtbuf, int sz);    // sys_open()からcall
int procfs_parse_usercontent(struct file *f);               // fileclose()からcall

#endif
