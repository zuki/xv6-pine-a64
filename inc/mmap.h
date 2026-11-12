#ifndef INC_MMAP_H
#define INC_MMAP_H

#include <types.h>
#include <file.h>

void *          mmap(uintptr_t addrp, size_t length, int prot, int flags, struct file* f, off_t offset);
int             munmap(uintptr_t addrp, size_t len);
void *          mremap(uintptr_t old_addr, size_t old_length, size_t new_length, int flags, uintptr_t new_addr);
int             mprotect(uintptr_t addr, size_t length, int prot);
int             msync(uintptr_t addr, size_t length, int flags);
uint64_t        get_perm(int prot, int flags);
int             mmap_load_pages(uintptr_t addr, size_t length, int prot, int flags, struct file *f, off_t offset);
void            print_mmap_list(struct task_struct *p, const char *title);
void            free_mmap_list(struct task_struct *p);
int             copy_vmas(struct task_struct *parent, struct task_struct *child);
struct vma *    find_vma(struct task_struct *p, uintptr_t start);
boolean         is_vma(struct task_struct *p, uintptr_t addr, uint64_t length);
int             alloc_mmap_page(struct task_struct *p, uintptr_t addr, uint64_t scause);
int             alloc_cow_page(struct mm_struct *mm, uintptr_t va);
int             copy_vma_pages(uintptr_t addr, size_t length, uint64_t perm);

#endif
