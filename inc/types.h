#ifndef INC_TYPES_H
#define INC_TYPES_H

#define NULL    ((void *)0)

#define CHAR_BIT    8
#define SCHAR_MIN   (-128)
#define SCHAR_MAX   127
#define UCHAR_MAX   255
#define SHRT_MIN    (-1-0x7fff)
#define SHRT_MAX    0x7fff
#define USHRT_MAX   0xffff
#define INT_MIN     (-1-0x7fffffff)
#define INT_MAX     0x7fffffff
#define UINT_MAX    0xffffffffU
#define LONG_MIN    (-LONG_MAX-1)
#define LONG_MAX    0x7fffffffffffffffL
#define ULONG_MAX   (2UL*LONG_MAX+1)
#define LLONG_MIN   (-LLONG_MAX-1)
#define LLONG_MAX   0x7fffffffffffffffLL
#define ULLONG_MAX  (2ULL*LLONG_MAX+1)
#define SIZE_MAX    ULLONG_MAX

typedef char            int8_t;
typedef unsigned char   uint8_t;
typedef short           int16_t;
typedef unsigned short  uint16_t;
typedef int             int32_t;
typedef unsigned int    uint32_t;
typedef long            int64_t;
typedef unsigned long   uint64_t;

typedef uint8_t         u8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef uint64_t        u64;
typedef int64_t         ssize_t;
typedef uint64_t        size_t;
typedef uint64_t        dev_t;
typedef uint32_t        device_t;
typedef uint32_t        major_t;
typedef uint32_t        minor_t;
typedef uint64_t        ino_t;
typedef uint64_t        lbaint_t;
typedef uint32_t        mode_t;
typedef uint32_t        nlink_t;
typedef int32_t         pid_t;
typedef uint32_t        uid_t;
typedef uint32_t        gid_t;
typedef int64_t         off_t;
typedef int32_t         blksize_t;
typedef int64_t         blkcnt_t;
typedef int64_t         time_t;
typedef int64_t         suseconds_t;
typedef uint32_t        tcflag_t;
typedef uint32_t        speed_t;
typedef uint8_t         cc_t;
typedef int32_t         clockid_t;
typedef uint64_t        ino64_t;
typedef off_t           off64_t;
typedef uint32_t        kernel_cap_t;
typedef uint64_t        uintptr_t;
typedef int64_t         intptr_t;
typedef unsigned        wint_t;
typedef uint16_t        wchar_t;        // muslでは4バイトだが、fatでは2バイト
typedef uint64_t        handler_t;
typedef uint32_t        socklen_t;

typedef int             boolean;

typedef unsigned char       uchar;
typedef unsigned short      ushort;
typedef unsigned int        uint;
typedef unsigned long       ulong;
typedef unsigned long long  uvlong;

typedef uint32_t            u32int;


#define true            1
#define false           0
#define bool            _Bool

#ifndef __ASSEMBLER__

/* Efficient min and max operations */
#define MIN(_a, _b)                 \
({                                  \
    typeof(_a) __a = (_a);          \
    typeof(_b) __b = (_b);          \
    __a <= __b ? __a : __b;         \
})
#define MAX(_a, _b)                 \
({                                  \
    typeof(_a) __a = (_a);          \
    typeof(_b) __b = (_b);          \
    __a >= __b ? __a : __b;         \
})


/* Round down and up to the nearest multiple of n */
#define ROUNDDOWN(a, n)                 \
({                                      \
    uint64_t __a = (uint64_t) (a);      \
    (typeof(a)) (__a - __a % (n));      \
})
#define ROUNDUP(a, n)                                           \
({                                                              \
    uint64_t __n = (uint64_t) (n);                              \
    (typeof(a)) (ROUNDDOWN((uint64_t) (a) + __n - 1, __n));     \
})

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))
#define ALIGN(p, n) (((p) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))
//#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))
#define IS_POWER_OF_2(n)    (!((n) & ((n)-1)))

#define offsetof(type, member) __builtin_offsetof(type, member)

#define container_of(ptr, type, member)                 \
({                                                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);  \
    (type *)((char *)__mptr - offsetof(type,member));   \
})

#define bit_add(p, n) ((p) |= (1U << (n)))
#define bit_remove(p, n) ((p) &= ~(1U << (n)))
#define bit_test(p, n) ((p) & (1U << (n)))

typedef __builtin_va_list va_list;

#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)
#define va_copy(d,s)    __builtin_va_copy(d,s)

#define PACKED          __attribute__ ((packed))
#define GALIGN(n)       __attribute__ ((aligned (n)))
#define NORETURN        __attribute__ ((noreturn))
#define NOOPT           __attribute__ ((optimize (0)))
#define STDOPT          __attribute__ ((optimize (2)))
#define MAXOPT          __attribute__ ((optimize (3)))
#define WEAK            __attribute__ ((weak))

#define major(x) (((x)>>8) & 0xff)
#define minor(x) ((x) & 0xff)

#define makedev(x,y) ((((x) & 0xff) << 8) | \
        ((y) & 0xff))

#ifndef BIT
#define BIT(x)  (1U << (x))
#endif

static inline uint16_t byteswap16(uint16_t v)
{
    return (v & 0x00ff) << 8 | (v & 0xff00 ) >> 8;
}

static inline uint32_t byteswap32(uint32_t v)
{
    return (v & 0x000000ff) << 24 | (v & 0x0000ff00) << 8 | (v & 0x00ff0000) >> 8 | (v & 0xff000000) >> 24;
}

static inline uint64_t byteswap64(uint64_t v)
{
    uint32_t vu, vl;
    vu = (uint32_t)((v & 0xffffffff00000000) >> 32);
    vl = (uint32_t)((v & 0xffffffff));
    return ((uint64_t)(byteswap32(vu)) | ((uint64_t)(byteswap32(vl)) << 32));
}

#endif // __ASSEMBLER__

#endif
