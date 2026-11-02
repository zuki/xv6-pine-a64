#ifndef INC_MMAP_H
#define INC_MMAP_H

#include <types.h>
#include <file.h>

void *          mmap(void *addr, size_t length, int prot, int flags, struct file* f, off_t offset);
long            munmap(void *addr, size_t len);
void *          mremap(void *old_addr, size_t old_length, size_t new_length, int flags, void *new_addr);
long            mprotect(void *addr, size_t length, int prot);
long            msync(void *addr, size_t length, int flags);
uint64_t        get_perm(int prot, int flags);
long            mmap_load_pages(void *addr, size_t length, int prot, int flags, struct file *f, off_t offset);
void            print_mmap_list(struct proc *p, const char *title);
void            free_mmap_list(struct proc *p);
long            copy_vmas(struct proc *parent, struct proc *child);
struct vma *    find_vma(struct proc *p, void *start);
boolean         is_vma(struct proc *p, void *addr, uint64_t length);
long            alloc_mmap_page(struct proc *p, uint64_t addr, uint64_t scause);
int             alloc_cow_page(uint64_t *pgdir, uint64_t va);
long            copy_vma_pages(void *addr, size_t length, uint64_t perm);

#endif
