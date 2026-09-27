#include <types.h>

#include <uart.h>
#include <arm.h>
#include <peripherals/uart.h>
#include <peripherals/gpio.h>
#include <irq.h>

void uart_putchar(int c)
{
    while (!(get32va(UART_LSR(0)) & UART_LSR_THRE))
        ;
    put32va(UART_THR(0), c & 0xFF);
}

char uart_getchar()
{
    while (!(get32va(UART_LSR(0)) & UART_LSR_DR))
        ;
    return get32va(UART_RBR(0)) & 0xFF;
}

void uart_init()
{
    uint32_t selector;

    put32va(BUS_CLK_GATING_REG3, get32va(BUS_CLK_GATING_REG3) | BUS_CLK_GATING_REG0_UART0_GATING);
    put32va(BUS_SOFT_RST_REG4, get32va(BUS_SOFT_RST_REG4) | BUS_SOFT_RST0_UART0_RST);

    selector = get32va(Pn_CFG1(1));
    selector &= ~(7<<4);
    selector |= 4<<4;
    selector &= ~(7<<0);
    selector |= 4<<0;
    put32va(Pn_CFG1(1), selector);

    while (get32va(UART_USR(0)) & UART_USR_BUSY)
        ;
    // ボーレートの設定
    selector = get32va(UART_LCR(0));
    selector |= UART_LCR_DLAB;
    put32va(UART_LCR(0), selector);
    put32va(UART_DLL(0), 13); // DL = 24MHz / 16 * 115200 = 13
    put32va(UART_DLH(0), 0);

    // 8N1の設定
    selector = get32va(UART_LCR(0));
    selector &= ~0x8f;  // clear DLAB and bits:3-0
    selector |= (UART_LCR_PEN | UART_LCR_STOP | UART_LCR_DLS);  // 8N1
    put32va(UART_LCR(0), selector);

    put32va(UART_IER(0), UART_ETBEI | UART_ERBFI);

    // FIFOは未設定
}
