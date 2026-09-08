#ifndef INC_CONSOLE_H
#define INC_CONSOLE_H

#include <types.h>
#include <arm.h>
#include <spinlock.h>

#define INPUT_BUF   128

struct serial_channel {
    char buf[INPUT_BUF];
    size_t r;                   // Read index
    size_t w;                   // Write index
    size_t e;                   // Edit index
    int opens;
    int open_mode;
    struct spinlock lock;
};

extern struct serial_channel g_channel;

void console_init(void);
void cprintf1(const char *fmt, ...);
void cprintf(const char *fmt, ...);
void panic(const char *f, int line, const char *fmt, ...);

#define assert(x)                                                   \
({                                                                  \
    if (!(x)) {                                                     \
        panic(__FILE__, __LINE__, "%s:%d: assertion failed.\n", __FILE__, __LINE__);    \
    }                                                               \
})

#define asserts(x, ...)                                             \
({                                                                  \
    if (!(x)) {                                                     \
        cprintf("%s:%d: assertion failed.\n", __FILE__, __LINE__);  \
        panic(__FILE__, __LINE__, __VA_ARGS__);                                         \
    }                                                               \
})

#define LOG1(_level, ...)                       \
({                                              \
    acquire(&g_channel.lock);                   \
    cprintf1("[%d]%s: ", cpuid(), __func__);    \
    cprintf1(__VA_ARGS__);                      \
    cprintf1("\n");                             \
    release(&g_channel.lock);                   \
})

#ifdef LOG_ERROR
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)
#define info(...)
#define debug(...)
#define trace(...)

#elif defined(LOG_WARN)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)
#define debug(...)
#define trace(...)

#elif defined(LOG_INFO)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)
#define trace(...)

#elif defined(LOG_DEBUG)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)  LOG1("DEBUG", __VA_ARGS__);
#define trace(...)

#elif defined(LOG_TRACE)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)  LOG1("DEBUG", __VA_ARGS__);
#define trace(...)  LOG1("TRACE", __VA_ARGS__);

#else
/* Default to none. */
#define error(...)
#define warn(...)
#define info(...)
#define debug(...)
#define trace(...)

#endif  // #ifdef LOG_ERROR

#endif

