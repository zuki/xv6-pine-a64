#include <types.h>
#include <string.h>
#include <linux/errno.h>
#include <console.h>
//#include <mm.h>

static int sprintint(int64_t x, int base, int sign, int zero, int col, char **p)
{
    static char digit[] = "0123456789abcdef";
    static char buf[64];
    int ret = 0;

    if (sign && x < 0) {
        x = -x;
        *(*p)++ = '-';
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
        else break;
    ret = i;
    while (i--)
        *(*p)++ = buf[i];
    return ret;
}

int vsnprintfmt(char *str, size_t size, const char *fmt, va_list ap)
{
    int i, c, j;
    char *s;
    char *p = str;
    int ret = 0;

    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (p - str > size) return -1;
        if (c != '%') {
            *p++ = c;
            ret++;
            continue;
        }

        int n = 0;
        int z = 0;
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

        int l = 0;
        for (; fmt[i + 1] == 'l'; i++)
            l++;

        if (!(c = fmt[++i] & 0xff))
            break;

        switch (c) {
        case 'u':
            if (l == 2)
                ret += sprintint(va_arg(ap, int64_t), 10, 0, z, n, &p);
            else
                ret += sprintint(va_arg(ap, uint32_t), 10, 0, z, n, &p);
            break;
        case 'd':
            if (l == 2)
                ret += sprintint(va_arg(ap, int64_t), 10, 1, z, n, &p);
            else
                ret += sprintint(va_arg(ap, int), 10, 1, z, n, &p);
            break;
        case 'x':
            if (l == 2)
                ret += sprintint(va_arg(ap, int64_t), 16, 0, z, n, &p);
            else
                ret += sprintint(va_arg(ap, uint32_t), 16, 0, z, n, &p);
            break;
        case 'p':
            ret += sprintint((uint64_t) va_arg(ap, void *), 16, 0, 0, 16, &p);
            break;
        case 'c':
            *p++ = (va_arg(ap, int));
            ret++;
            break;
        case 's':
            j = 0;
            if ((s = (char*)va_arg(ap, char *)) == 0) {
                s = "(null)";
                j = 6;
            }
            for (; *s; s++) {
                *p++ = *s;
                j++;
                ret++;
            }
            if (n > j) {
                n = n - j;
                while(n--) {
                    *p++ = ' ';
                    ret++;
                }
            }
            break;
        case '%':
            *p++ = '%';
            ret++;
            break;
        default:
            /* Print unknown % sequence to draw attention. */
            *p++ = '%';
            *p++ = c;
            ret += 2;
            break;
        }
    }
    *p = '\0';
    return ret;
}

static int vsprintfmt(char *str, size_t n, const char *fmt, va_list ap)
{
    return vsnprintfmt(str, n, fmt, ap);
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = vsprintfmt(buf, n, fmt, ap);
    va_end(ap);

    return rc;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = vsprintfmt(buf, 64, fmt, ap);
    va_end(ap);

    return rc;
}

char *safestrcpy(char *s, const char *t, size_t n)
{
    char *os = s;
    if (n <= 0)
        return os;
    while (--n > 0 && (*s++ = *t++) != 0)
        ;
    *s = 0;
    return os;
}

long strtol(const char *s, char **endptr, int base)
{
    int neg = 0;
    long val = 0;

    // gobble initial whitespace
    while (*s == ' ' || *s == '\t')
        s++;

    // plus/minus sign
    if (*s == '+')
        s++;
    else if (*s == '-')
        s++, neg = 1;

    // hex or octal base prefix
    if ((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x'))
        s += 2, base = 16;
    else if (base == 0 && s[0] == '0')
        s++, base = 8;
    else if (base == 0)
        base = 10;

    // digits
    while (1) {
        int dig;

        if (*s >= '0' && *s <= '9')
            dig = *s - '0';
        else if (*s >= 'a' && *s <= 'z')
            dig = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z')
            dig = *s - 'A' + 10;
        else
            break;
        if (dig >= base)
            break;
        s++, val = (val * base) + dig;
        // we don't properly detect overflow!
    }

    if (endptr)
        *endptr = (char *) s;
    return (neg ? -val : val);
}

long long strtoll(const char *s, char **endptr, int base)
{
    return (long long)strtol(s, endptr, base);
}


unsigned long long strtoull(const char *s, char **endptr, int base)
{
    return (unsigned long long)strtol(s, endptr, base);
}

int atoi(const char *s)
{
    int n = 0, neg = 0;

    while (*s == ' ' || *s == '\t')
        s++;
    switch (*s) {
        case '-':   neg = 1;
        case '+':   s++;
    }

    while (isdigit(*s))
        n = 10 * n - (*s++ - '0');
    return neg ? n : -n;
}

// 以下、mustのコードを借用

void *memchr(const void *src, int c, size_t n)
{
    const unsigned char *s = src;
    c = (unsigned char)c;
    for (; n && *s != c; s++, n--);
    return n ? (void *)s : 0;
}

void *memmove(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;

    if (d==s) return d;
    if ((uintptr_t)s-(uintptr_t)d-n <= -2*n) return memcpy(d, s, n);

    if (d<s) {
        for (; n; n--) *d++ = *s++;
    } else {
        while (n) n--, d[n] = s[n];
    }

    return dest;
}

int memcmp(const void *vl, const void *vr, size_t n)
{
    const unsigned char *l=vl, *r=vr;
    for (; n && *l == *r; n--, l++, r++);
    return n ? *l-*r : 0;
}

int strncmp(const char *_l, const char *_r, size_t n)
{
    const unsigned char *l=(void *)_l, *r=(void *)_r;
    if (!n--) return 0;
    for (; *l && *r && n && *l == *r ; l++, r++, n--);
    return *l - *r;
}

static char *__stpcpy(char *restrict d, const char *restrict s)
{
    for (; (*d=*s); s++, d++);

    return d;
}

static char *__stpncpy(char *restrict d, const char *restrict s, size_t n)
{
    for (; n && (*d=*s); n--, s++, d++);
    memset(d, 0, n);
    return d;
}

char *strcpy(char *restrict dest, const char *restrict src)
{
    __stpcpy(dest, src);
    return dest;
}



char *strncpy(char *restrict d, const char *restrict s, size_t n)
{
    __stpncpy(d, s, n);
    return d;
}

size_t strlen(const char *s)
{
    const char *a = s;
    for (; *s; s++);
    return s-a;
}



int strcmp(const char *l, const char *r)
{
    for (; *l==*r && *l; l++, r++);
    return *(unsigned char *)l - *(unsigned char *)r;
}

static char *__strchrnul(const char *s, int c)
{
    c = (unsigned char)c;
    if (!c) return (char *)s + strlen(s);

    for (; *s && *(unsigned char *)s != c; s++);
    return (char *)s;
}

#define BITOP(a,b,op) \
 ((a)[(size_t)(b)/(8*sizeof *(a))] op (size_t)1<<((size_t)(b)%(8*sizeof *(a))))

size_t strspn(const char *s, const char *c)
{
    const char *a = s;
    size_t byteset[32/sizeof(size_t)] = { 0 };

    if (!c[0]) return 0;
    if (!c[1]) {
        for (; *s == *c; s++);
        return s-a;
    }

    for (; *c && BITOP(byteset, *(unsigned char *)c, |=); c++);
    for (; *s && BITOP(byteset, *(unsigned char *)s, &); s++);
    return s-a;
}

size_t strcspn(const char *s, const char *c)
{
    const char *a = s;
    size_t byteset[32/sizeof(size_t)];

    if (!c[0] || !c[1]) return __strchrnul(s, *c)-a;

    memset(byteset, 0, sizeof byteset);
    for (; *c && BITOP(byteset, *(unsigned char *)c, |=); c++);
    for (; *s && !BITOP(byteset, *(unsigned char *)s, &); s++);
    return s-a;
}

char *strchr(const char *s, int c)
{
    char *r = __strchrnul(s, c);
    return *(unsigned char *)r == (unsigned char)c ? r : 0;
}

static void *__memrchr(const void *m, int c, size_t n)
{
    const unsigned char *s = m;
    c = (unsigned char)c;
    while (n--) if (s[n]==c) return (void *)(s+n);
    return 0;
}

char *strrchr(const char *s, int c)
{
    return __memrchr(s, c, strlen(s) + 1);
}

char *strtok_r(char *s, const char *sep, char **p)
{
    if (!s && !(s = *p)) return NULL;
    s += strspn(s, sep);
    if (!*s) return *p = 0;
    *p = s + strcspn(s, sep);
    if (**p) *(*p)++ = 0;
    else *p = 0;
    return s;
}

int strcasecmp(const char *_l, const char *_r)
{
    const unsigned char *l=(void *)_l, *r=(void *)_r;
    for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++);
    return tolower(*l) - tolower(*r);
}

int strncasecmp(const char *_l, const char *_r, size_t n)
{
    const unsigned char *l=(void *)_l, *r=(void *)_r;
    if (!n--) return 0;
    for (; *l && *r && n && (*l == *r || tolower(*l) == tolower(*r)); l++, r++, n--);
    return tolower(*l) - tolower(*r);
}

// FIXME: kmallocを実装したらコメントアウト
#if 0
char *strdup(const char *s)
{
    size_t l = strlen(s);
    char *d = kmalloc(l+1);
    if (!d) return NULL;
    return memcpy(d, s, l+1);
}
#endif
// 以上、muslのコード借用終わり

// CString::Replaceの挙動を模倣する関数
// 引数:
//   source : 文字列ポインタのポインタ (reallocでアドレスが変わる可能性があるため)
//   find   : 検索する文字列
//   replace: 置換する文字列
// 戻り値: 置換した個数
// FIXME: kmallocを実装したらコメントアウト
#if 0
int str_replace(char **source, const char *find, const char *replace) {
    if (source == NULL || *source == NULL || find == NULL || replace == NULL) {
        return 0;
    }
    trace("source: %s, find: %s, replace: %s", *source, find, replace);
    size_t find_len = strlen(find);
    if (find_len == 0) {
        return 0; // 検索文字列が空の場合は何もしない
    }
    size_t replace_len = strlen(replace);

    // 1. まず置換が何箇所あるかカウントし、必要なメモリサイズを計算する
    int count = 0;
    const char *tmp = *source;
    while ((tmp = strstr(tmp, find)) != NULL) {
        count++;
        tmp += find_len; // 次の検索へ進む
    }

    // 置換対象が見つからなければ、何もせず0を返す
    if (count == 0) {
        return 0;
    }

    // 2. 新しい文字列の長さを計算してメモリを確保
    size_t source_len = strlen(*source);
    // 新サイズ = 元の長さ + (置換後の長さ - 置換前の長さ) * 置換個数
    size_t new_len = source_len + (replace_len - find_len) * count;

    char *new_str = (char *)kmalloc(new_len + 1);
    if (new_str == NULL) {
        return -1;
    }

    // 3. 文字列の構築（コピーと置換）
    char *dst = new_str;
    const char *src = *source;
    while ((tmp = strstr(src, find)) != NULL) {
        // 検索ヒット位置までの文字列をコピー
        size_t len = tmp - src;
        memcpy(dst, src, len);
        dst += len;

        // 置換文字列をコピー
        memcpy(dst, replace, replace_len);
        dst += replace_len;

        // 次の検索位置へ進む
        src = tmp + find_len;
    }
    // 残りの末尾文字列をコピー
    strncpy(dst, src, strlen(src));

    // 4. 古いメモリを解放し、新しいメモリ空間に差し替える
    kmfree(*source);
    *source = new_str;
    trace("new source: %s", *source);
    return count; // 置換した個数を返す
}



void *memset(void *v, int c, size_t n)
{
    char *p;
    int m;

    p = v;
    m = n;
    while (--m >= 0)
        *p++ = c;

    return v;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    return memmove(dst, src, n);
}
#endif
