#ifndef INC_STRING_H
#define INC_STRING_H

#include <types.h>
#include <arm.h>

static inline void format(uint8_t c, char *sc) {
    const char digit[] = "0123456789abcdef";
    sc[0] = digit[c >> 4];
    sc[1] = digit[c & 0xf];
}

int sprintf(char *str, const char *fmt, ...);

static __inline int __isspace(int _c)
{
    return _c == ' ' || (unsigned)_c-'\t' < 5;
}

// 以下は <ctype.h>の関数
static inline int abs(int i) { return i < 0 ? -i : i; }
static inline int isblank(int c) { return c == ' ' || c == '\t'; }

static inline int sscanf(const char *str, const char *format, ...)
{
    return 0;
}

#define isascii(a) ((a >= 0x00) && (a <= 0x7f))
#define isprint(a) ((a >= 0x20) && (a <= 0x7e))

#define isalpha(a) ((((unsigned)(a)|32)-'a') < 26)
#define isdigit(a) (((unsigned)(a)-'0') < 10)
#define islower(a) (((unsigned)(a)-'a') < 26)
#define isupper(a) (((unsigned)(a)-'A') < 26)
#define isgraph(a) (((unsigned)(a)-0x21) < 0x5e)
#define isspace(a) __isspace(a)

static inline int tolower(int c) {
    if (isupper(c)) return c | 32;
    return c;
}

static inline int toupper(int c) {
	if (islower(c)) return c & 0x5f;
	return c;
}

int atoi(const char *s);
long strtol(const char *s, char **endptr, int base);
long long strtoll(const char *s, char **endptr, int base);
unsigned long long strtoull(const char *s, char **endptr, int base);

// <string.h>関数

void *memchr(const void *src, int c, size_t n);
int memcmp(const void *vl, const void *vr, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

void *memset(void *v, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);

char *strcpy(char *restrict dest, const char *restrict src);
char *strncpy(char *restrict d, const char *restrict s, size_t n);
char *safestrcpy(char *s, const char *t, size_t n);
size_t strlen(const char *s);

int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
char *strncpy(char *restrict d, const char *restrict s, size_t n);

int strcmp(const char *l, const char *r);
int strncmp(const char *_l, const char *_r, size_t n);
size_t strspn(const char *s, const char *c);
size_t strcspn(const char *s, const char *c);
char *strtok_r(char *s, const char *sep, char **p);
int str_replace(char **source, const char *find, const char *replace);
int strcasecmp(const char *_l, const char *_r);
int strncasecmp(const char *_l, const char *_r, size_t n);
char *strdup (const char *s);
int vsnprintfmt(char *str, size_t size, const char *fmt, va_list ap);
//char *strchrnul(const char *s, int c);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *h, const char *n);

#endif
