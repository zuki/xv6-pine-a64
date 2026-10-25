#ifndef INC_UTILS_H
#define INC_UTILS_H

void __asm_flush_dcache_range(void *start, void *end);
void __asm_flush_dcache_all(int invalidate_only);
void __asm_invalidate_dcache_range(void *start, void *end);
void __asm_dcache_level(int level);
void __asm_invalidate_dcache_all(void);
void __asm_flush_dcache_line(void *addr);
void __asm_invalidate_dcache_line(void *addr);

#endif
