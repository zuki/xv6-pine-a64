#ifndef KERN_VM_H
#define KERN_VM_H

#include <memlayout.h>

//void vm_free(uint64_t *pgdir, int level);

void *allocate_user_page_mm(struct mm_struct *mm, uint64_t va, uint64_t perm);
uint64_t *map_page(struct mm_struct *mm, uint64_t va, uint64_t page, int alloc, uint64_t perm);
int dup_current_virt_memory(struct mm_struct *dstmm);
uint64_t walkaddr(struct mm_struct *mm, uint64_t va);
int copyout(struct mm_struct *mm, uint64_t dstva, char *src, uint64_t len);
int copyin(struct mm_struct *mm, char *dst, uint64_t srcva, uint64_t len);
int copyinstr(struct mm_struct *mm, char *dst, uint64_t srcva, uint64_t max);
int either_copyout(int user_dst, uint64_t dst, void *src, uint64_t len);
int either_copyin(void *dst, int user_src, uint64_t src, uint64_t len);
void free_task_pages(struct mm_struct *mm, int useronly);
uint64_t growproc(struct mm_struct *mm, int incr);
int do_mem_abort(uint64_t addr, uint64_t esr, uint64_t elr);

#endif /* !KERN_VM_H */
