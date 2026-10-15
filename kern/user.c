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

// cannot call kernel's strlen
 __attribute__((unused)) static unsigned int user_strlen(const char *s)
{
  int n;
  for(n = 0; s[n]; n++)
    ;
  return n;
}

void print_to_console(char *msg) {
    call_sys_write(1 /*stdout*/, msg, user_strlen(msg));
}


#define USER_PROGRAM1(name, arg0, arg1) \
    char path[] = {name};               \
    char argv0[] = {arg0};              \
    char argv1[] = {arg1};              \
    char *argv[] = {argv0, argv1, 0};

// スタンドアロンユーザプログラム用の「ランチャー」
void user_process() {
    USER_PROGRAM1("/sh", "/sh", "");

    char console[] = {"/dev/console"};
    if (call_sys_open(AT_FDCWD, console, O_CREAT | O_EXCL, O_RDWR) < 0) {
        call_sys_mknod(AT_FDCWD, console, S_IFCHR, CONSOLE << 8);
        call_sys_open(AT_FDCWD, console, 0, O_RDWR);
    }
    call_sys_dup(0); // stdout
    call_sys_dup(0); // stderr

    char msg[] = {"User process entry\n"};
    print_to_console(msg);

    if (call_sys_exec(path, argv, 0) != 0)
        print_to_console("exec user program failed\n");
}
