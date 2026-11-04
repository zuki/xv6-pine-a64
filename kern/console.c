#include <types.h>
#include <param.h>
#include <arm.h>
#include <console.h>
#include <file.h>
#include <irq.h>
#include <spinlock.h>
#include <uart.h>
#include <vm.h>
#include <proc.h>

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
    int c, prof = 0;

    //acquire(&g_channel.lock);
    if (panicked >= 0) {
        //release(&g_channel.lock);
        while (1) ;
    }

    while ((c = getc()) >= 0) {
        switch (c) {
        case C('P'):           // Process listing.
            prof = 1;
            break;
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
            if (c != 0 && (g_channel.e - g_channel.r) < INPUT_BUF) {
                c = (c == '\r') ? '\n' : c;
                g_channel.buf[g_channel.e++ % INPUT_BUF] = c;
                consputc(c);
                if (c == '\n' || c == C('D')
                    || (g_channel.e - g_channel.r) == INPUT_BUF) {
                    g_channel.w = g_channel.e;
                    wakeup(&g_channel.r);
                }
            }
            break;
        }
    }
    //release(&g_channel.lock);

    if (prof) {
        //mm_dump();
        procdump();
    }
}

static ssize_t consolewrite(int user_src, uint64_t src, int off, int n)
{
    for (int i = 0; i < n; i++) {
        char c;
        if (either_copyin(&c, user_src, src + i, 1) < 0)
            break;
        if (c == '\1')
            c = 0x20;
        uart_putchar(c);
    }
    return n;
}

static ssize_t consoleread(int user_dst, uint64_t dst, int off, int n)
{
    uint32_t bufsize = n;

    trace("user_dst: %d, dst: 0x%lx, off: %ld, n: %d",
        user_dst, dst, off, n);

    acquire(&g_channel.lock);
    while (n > 0) {
        while (g_channel.r == g_channel.w) {
            if (thisproc()->killed) {
                release(&g_channel.lock);
                return -1;
            }
            sleep(&g_channel.r, &g_channel.lock);
        }
        int c = g_channel.buf[g_channel.r++ % INPUT_BUF];
        if (c == C('D')) {
            if (n < bufsize)
                g_channel.r--;
            break;
        }
        char cbuf = (c & 0xff);
        if (either_copyout(user_dst, dst, &cbuf, 1) < 0)
            break;

        ++dst;
        --n;

        if (c == '\n')
            break;
    }
    release(&g_channel.lock);
    trace("return %d", bufsize - n);
    return bufsize - n;
}

void console_intr(void *_p)
{
    console_intr1(uart_getchar);
}

void console_init(void)
{
    initlock(&g_channel.lock, "console");
    uart_init();

    devsw[TTYMAJOR].read = consoleread;
    devsw[TTYMAJOR].write = consolewrite;
    //devsw[TTYMAJOR].ioctl = consolectl;

    set_gicd_itarget(IRQ_UART0, 0x0f);          // すべてのcpuに送信
    irq_register(IRQ_UART0, console_intr, 0);
    irq_enable(IRQ_UART0);

    //debug("GICD_ITARGETSR[1]: 0x%x", get32(GICD_ITARGETSRn+32));

    info("console_init ok");
}

static void printint(int64_t x, int base, int sign, int zero, int col)
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

    for (; i < col; i++)
        if (zero == 1)
            buf[i] = '0';
        else if (zero == -1)
            buf[i] = ' ';
        else
            break;

    while (i--) uart_putchar(buf[i]);
}

void vprintfmt(void (*putch)(int), const char *fmt, va_list ap)
{
    int i, c, j;
    char *s;
    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (c != '%') {
            putch(c);
            continue;
        }

        int l = 0, n = 0, z = 0;
        if (fmt[i+1] == '0') {
            z = 1;
            i++;
        } else if (fmt[i+1] == '-') {
            z = -1;
            i++;
        }

        for (; fmt[i+1] >= '0' && fmt[i+1] <= '9'; i++) {
            n = n * 10 + (fmt[i+1] - '0') % 10;
        }

        for (; fmt[i+1] == 'l'; i++)
            l++;

        if (!(c = fmt[++i] & 0xff))
            break;

        switch (c) {
        case 'u':
            if (l > 0) printint(va_arg(ap, int64_t), 10, 0, z, n);
            else printint(va_arg(ap, uint32_t), 10, 0, z, n);
            break;
        case 'd':
            if (l > 0) printint(va_arg(ap, int64_t), 10, 1, z, n);
            else printint(va_arg(ap, int), 10, 1, z, n);
            break;
        case 'x':
            if (l > 0) printint(va_arg(ap, int64_t), 16, 0, z, n);
            else printint(va_arg(ap, uint32_t), 16, 0, z, n);
            break;
        case 'p':
            putch('0');
            putch('x');
            printint((int64_t)va_arg(ap, void *), 16, 0, 0, 16);
            break;
        case 'c':
            putch(va_arg(ap, int));
            break;
        case 's':
            j = 0;
            if ((s = (char*)va_arg(ap, char *)) == 0) {
                s = "(null)";
                j = 6;
            }
            for (; *s; s++) {
                putch(*s);
                j++;
            }
            if (n > j) {
                n = n - j;
                while(n--) {
                    putch(' ');
                }
            }
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
    trace("acq g_chan");
    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);
    release(&g_channel.lock);
    trace("rel g_chan");
}

void cprintf1(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);
}

#if 0
void panic(const char *fmt, ...)
{
    va_list ap;

    acquire(&g_channel.lock);
    if (panicked < 0)
        panicked = cpuid();
    else {
        release(&g_channel.lock);
        while (1) ;
    }
    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);
    release(&g_channel.lock);

    cprintf("%s:%d: kernel panic at cpu %d.\n", __FILE__, __LINE__,
            cpuid());
    while (1) ;
}
#endif
