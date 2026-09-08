#include <types.h>
#include <arm.h>
#include <string.h>
#include <console.h>
#include <kalloc.h>
#include <irq.h>
#include <timer.h>
#include <trap.h>

extern void *vectors;

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
    irq_init();
    load_vbar_el1(vectors);
    timer_init();

    while (1) ;
}
