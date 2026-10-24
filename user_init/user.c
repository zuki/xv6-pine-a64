// ユーザタスク用のカーネルランチャ

// カーネルにコンパイルされるが、EL0かつ独自の仮想アドレス空間で実行される
//「疑似」ユーザタスクはシステムコールを除いていかなるカーネル関数も呼び出すことが
// できない――そうするとメモリ保護エラーが発生する
//
// 一般に、プログラミング環境は非常に制限されている。なぜなら、すべて（例: printf()）を
// 再実装する必要があるからである

// 注意すべき点は
// 1. 参照されるシンボルは、user_begin/user_end の間のこのユニット内にコンパイル
//    されていなければならない。たとえば、外部で定義されたカーネル関数を呼び出すことはできない。
//    外部でリンクされる可能性のある定数文字列（？）を参照することもできない
// 2. シンボルへのすべての参照は、PC相対でなければならない。絶対アドレスではいけない。
//    Cコンパイラをそれほど細かく制御できないことが多いため、これはさらに困難である。
//    したがって、-O0 オプションでは多くの場合動作するが、-O2 では動作しないことがある。
//    アセンブリ言語で記述する場合、リンカがどこに配置するかを制御できないようである。
//    そのため、リンカはオフセットをエンコードできないとエラーを出すことがよくある。
//     cf stawged/testuser_launch.S and user-test.c

#include <user_sys.h>
#include <linux/fcntl.h>
#include <linux/stat.h>

#define USER_PROGRAM1(name, arg0)  \
    char path[] = {name};            \
    char argv0[] = {arg0};            \
    char *argv[] = {argv0, 0};

// スタンドアロンユーザプログラム用の「ランチャー」
void user_process() {
    USER_PROGRAM1("/bin/sh", "/bin/sh");
    int ret;

    char console[] = {"/dev/console"};
    if (call_sys_open(AT_FDCWD, console, 0, O_RDWR) < 0) {
        call_sys_mknod(AT_FDCWD, console, S_IFCHR, CONSOLE << 8);
        call_sys_open(AT_FDCWD, console, 0, O_RDWR);
    }
    call_sys_dup(0); // stdout
    call_sys_dup(0); // stderr
    if ((ret = call_sys_exec(path, argv, 0)) < 0) {
        call_sys_exit(ret);
    }
}
