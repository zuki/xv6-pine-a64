#ifndef KERN_KALLOC_H
#define KERN_KALLOC_H

void alloc_init(void);
char *kalloc(void);
void kfree(char *v);
void free_range(void *vstart, void *vend);
void check_free_list(void);

#endif /* !KERN_KALLOC_H */
