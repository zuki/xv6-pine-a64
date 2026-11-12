#ifndef INC_CACHEPAGE_H
#define INC_CACHEPAGE_H

#include <types.h>
#include <list.h>
#include <file.h>
#include <sleeplock.h>

#define NPAGECACHE 24576        // 0x6000 * 0x1000 (4096) = 0x600_0000 = 96MB

struct cachepage {
    char *      page;           // 0
    ino_t       ino;            // 8
    device_t    dev;            // 16
    int         ref_count;      // 20
    off_t       offset;         // 24
    struct sleeplock lock;      // 32
};                              // 56

void cachepage_init(void);
struct cachepage *get_cachepage(struct file *file, off_t offset);
void update_cachepage(device_t dev, ino_t ino, off_t offset, char *addr, size_t size);
long copy_cachepage(struct file *file, off_t offset, char *dest, size_t size, off_t dest_offset);
long copy_cachepages(struct file *file, char *dest, size_t size, off_t offset);
#endif
