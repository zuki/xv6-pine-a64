#include <types.h>
#include <string.h>
#include <mmu.h>
#include <memlayout.h>
#include <console.h>
#include <kalloc.h>
#include <spinlock.h>

extern char kernel_end[];

/*
 * Free page's list element struct.
 * We store each free page's run structure in the free page itself.
 */
struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *free_list; /* Free list of physical pages */
} kmem;

void alloc_init(void)
{
    initlock(&kmem.lock, "kmem");
    free_range(kernel_end, P2V(PHYS_TOP));
    //info("alloc_init ok");
}

/* Free the page of physical memory pointed at by v. */
void kfree(char *v)
{
    struct run *r;

    if ((uint64_t)v % PGSIZE || v < kernel_end || V2P(v) >= PHYS_TOP)
        panic("kfree");

    /* Fill with junk to catch dangling refs. */
    //memset(v, 1, PGSIZE);     // ここでメモリクリアすると非常に時間がかかるのでしない

    r = (struct run *)v;
    acquire(&kmem.lock);
    r->next = kmem.free_list;
    kmem.free_list = r;
    release(&kmem.lock);
}

void free_range(void *vstart, void *vend)
{
    char *p;

    p = ROUNDUP((char *)vstart, PGSIZE);
    //cprintf("free_range: start: %p, end: %p\n", p, vend);
    for (; p + PGSIZE <= (char *)vend; p += PGSIZE) {
        kfree(p);
    }
}

/*
 * Allocate one 4096-byte page of physical memory.
 * Returns a pointer that the kernel can use.
 * Returns 0 if the memory cannot be allocated.
 */
char *kalloc(void)
{
    struct run *p;

    acquire(&kmem.lock);
    p = kmem.free_list;
    if (p)
        kmem.free_list = p->next;
    release(&kmem.lock);
    return (char *)p;
}

void check_free_list(void)
{
    struct run *p;
    if (!kmem.free_list)
        panic("'kmem.free_list' is a null pointer!");

    for (p = kmem.free_list; p; p = p->next) {
        assert((void *)p > (void *)kernel_end);
    }
    info("check_free_list ok");
}
