#include <types.h>
#include <arm.h>
#include <console.h>
#include <irq.h>
#include <spinlock.h>
#include <uart.h>

struct serial_channel g_channel;
static int panicked = -1;

#define C(x)  ((x)-'@')         // Control-x
#define BACKSPACE 0x100

static void consputc(int c)
{
    if (c == BACKSPACE) {
        uart_putchar('\b');
        uart_putchar(' ');
        uart_putchar('\b');
    } else {
        uart_putchar(c);
    }
}

static void console_intr1(char (*getc)())
{
    int c; /*, prof = 0; */

    acquire(&g_channel.lock);
    if (panicked >= 0) {
        release(&g_channel.lock);
        while (1) ;
    }

    while ((c = getc()) >= 0) {
        switch (c) {
        //case C('P'):           // Process listing.
        //    prof = 1;
        //    break;
        case C('U'):           // Kill line.
            while (g_channel.e != g_channel.w
                   && g_channel.buf[(g_channel.e - 1) % INPUT_BUF] != '\n') {
                g_channel.e--;
                consputc(BACKSPACE);
            }
            break;
        case C('H'):
        case '\x7f':           // Backspace
            if (g_channel.e != g_channel.w) {
                g_channel.e--;
                consputc(BACKSPACE);
            }
            break;
        default:
            if (c != 0 && g_channel.e - g_channel.r < INPUT_BUF) {
                c = (c == '\r') ? '\n' : c;
                g_channel.buf[g_channel.e++ % INPUT_BUF] = c;
                consputc(c);
                if (c == '\n' || c == C('D')
                    || g_channel.e == g_channel.r + INPUT_BUF) {
                    g_channel.w = g_channel.e;
                    //wakeup(&g_channel.r);
                }
            }
            break;
        }
    }
    release(&g_channel.lock);

#if 0
    if (prof) {
        //mm_dump();
        procdump();
    }
#endif
}

void console_intr(void *_p)
{
    console_intr1(uart_getchar);
}

void console_init(void)
{
    initlock(&g_channel.lock, "console");
    uart_init();

    irq_register(IRQ_UART0, console_intr, 0);
    irq_enable(IRQ_UART0);
}

static void printint(int64_t x, int base, int sign)
{
    static char digit[] = "0123456789abcdef";
    static char buf[64];

    if (sign && x < 0) {
        x = -x;
        uart_putchar('-');
    }

    int i = 0;
    uint64_t t = x;
    do {
        buf[i++] = digit[t % base];
    } while (t /= base);

    while (i--) uart_putchar(buf[i]);
}

void vprintfmt(void (*putch)(int), const char *fmt, va_list ap)
{
    int i, c;
    char *s;
    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (c != '%') {
            putch(c);
            continue;
        }

        int l = 0;
        for (; fmt[i+1] == 'l'; i++)
            l++;

        if (!(c = fmt[++i] & 0xff))
            break;

        switch (c) {
        case 'u':
            if (l == 2) printint(va_arg(ap, int64_t), 10, 0);
            else printint(va_arg(ap, int), 10, 0);
            break;
        case 'd':
            if (l == 2) printint(va_arg(ap, int64_t), 10, 1);
            else printint(va_arg(ap, int), 10, 1);
            break;
        case 'x':
            if (l == 2) printint(va_arg(ap, int64_t), 16, 0);
            else printint(va_arg(ap, int), 16, 0);
            break;
        case 'p':
            putch('0');
            putch('x');
            printint((int64_t)va_arg(ap, void *), 16, 0);
            break;
        case 'c':
            putch(va_arg(ap, int));
            break;
        case 's':
            if ((s = (char*)va_arg(ap, char *)) == 0)
                s = "(null)";
            for (; *s; s++)
                putch(*s);
            break;
        case '%':
            putch('%');
            break;
        default:
            /* Print unknown % sequence to draw attention. */
            putch('%');
            putch(c);
            break;
        }
    }
}

/* Print to the console. */
void cprintf(const char *fmt, ...)
{
    va_list ap;

    acquire(&g_channel.lock);
    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);
    release(&g_channel.lock);
}

void cprintf1(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);
}
