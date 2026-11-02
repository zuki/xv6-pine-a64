#ifndef INC_VM_H
#define INC_VM_H

#include <types.h>
#include <proc.h>

uint64_t *  vm_init(void);
uint64_t *  walk(uint64_t * pgdir, void *vap, int alloc);
uint64_t    walkaddr(uint64_t *pgdir, uint64_t va);
void        vm_free(uint64_t *pgdir);
uint64_t *  uvm_copy(uint64_t *pgdir);
void        uvm_switch(uint64_t *pgdir);
int         uvm_map(uint64_t *pgdir, void *va, size_t sz, uint64_t pa);
void        uvm_unmap(uint64_t *pgdir, uint64_t va, uint64_t npages, int do_free);
int         uvm_alloc(uint64_t *pgdir, size_t base, size_t stksz, size_t oldsz, size_t newsz);
int         uvm_dealloc(uint64_t *pgdir, size_t base, size_t oldsz, size_t newsz);
int         copyout(uint64_t * pgdir, void *dstva, void *src, size_t len);
int         copyin(uint64_t *pgdir, char *dst, uint64_t srcva, uint64_t len);
int         copyinstr(uint64_t *pgdir, char *dst, uint64_t srcva, uint64_t max);
int         either_copyout(int user_dst, uint64_t dst, void *src, uint64_t len);
int         either_copyin(void *dst, int user_src, uint64_t src, uint64_t len);
void        vm_stat(uint64_t *pgdir);
void        vm_test(void);

#endif
