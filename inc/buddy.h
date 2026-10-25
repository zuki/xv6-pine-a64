#ifndef INC_BUDDY_H
#define INC_BUDDY_H

#include <types.h>
#include <mm.h>

void            buddy_init(void);
struct page *   buddy_alloc(size_t size);
void            buddy_free(struct page *page);

#endif
