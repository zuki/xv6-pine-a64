#include <types.h>

#include <string.h>
#include <console.h>
#include <kalloc.h>

void main(void)
{
    // 1文字デバッグ出力
/*
    *(volatile uint32_t *)0x01c28000 = 'M';

    // メモリバリア（C言語のインラインアセンブラ）
    asm volatile("dsb sy" : : : "memory");
    asm volatile("isb" : : : "memory");
*/
    /*
     * Before doing anything else, we need to ensure that all
     * static/global variables start out zero.
     */
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    console_init();
    alloc_init();
    cprintf("Allocator: Init success.\n");
    check_free_list();
    cprintf("check_free_list ok.\n");

    while (1) ;
}
