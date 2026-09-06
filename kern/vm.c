#include <types.h>
#include <mmu.h>
#include <string.h>
#include <memlayout.h>
#include <console.h>

#include <vm.h>
#include <kalloc.h>

/*
 * If the page is invalid, then allocate a new one. Return NULL if failed.
 */
static uint64_t *pde_validate(uint64_t *pde, int64_t alloc)
{
    if (!(*pde & PTE_P)) {  // if the page is invalid
        if (!alloc) return NULL;
        char *p = kalloc();
        if (!p) return NULL;  // allocation failed
        memset(p, 0, PGSIZE);
        *pde = V2P(p) | PTE_P | PTE_PAGE | PTE_USER | PTE_RW;
    }
    return pde;
}

/*
 * Given 'pgdir', a pointer to a page directory, pgdir_walk returns
 * a pointer to the page table entry (PTE) for virtual address 'va'.
 * This requires walking the four-level page table structure.
 *
 * The relevant page table page might not exist yet.
 * If this is true, and alloc == false, then pgdir_walk returns NULL.
 * Otherwise, pgdir_walk allocates a new page table page with kalloc.
 *   - If the allocation fails, pgdir_walk returns NULL.
 *   - Otherwise, the new page is cleared, and pgdir_walk returns
 *     a pointer into the new page table page.
 */

static uint64_t *pgdir_walk(uint64_t *pgdir, const void *va, int64_t alloc)
{
    uint64_t *pde = pgdir;

    for (int l = 0; l < 3; l++) {
        pde = &pde[PTX(l, va)];
        if (!(pde = pde_validate(pde, alloc)))
            return NULL;
        pde = (uint64_t *)P2V(PTE_ADDR(*pde));
    }
    return &pde[L3X(va)];
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might **NOT**
 * be page-aligned.
 * Use permission bits perm|PTE_P|PTE_TABLE|PTE_AF for the entries.
 *
 * Hint: call pgdir_walk to get the corresponding page table entry
 */

static int map_region(uint64_t *pgdir, void *va, uint64_t size, uint64_t pa, int64_t perm)
{
    void *vap = (void *)PGROUNDDOWN((uint64_t)va);
    void *end = va + size;
    pa = PGROUNDDOWN(pa);

    for (; vap < end; pa += PGSIZE, vap += PGSIZE) {
        uint64_t *pte = pgdir_walk(pgdir, vap, 1);
        if (!pte) {
            cprintf("walk failed\n");
            return 1;
        }
        if (*pte & PTE_P)
            panic("remap");
        *pte = pa | perm | PTE_P | PTE_TABLE | (MT_NORMAL << 2) | PTE_AF | PTE_SH;
    }

    return 0;
}

/*
 * Free a page table.
 *
 * Hint: You need to free all existing PTEs for this pgdir.
 */

void vm_free(uint64_t *pgdir, int level)
{
    if (!pgdir || level < 0 || level > 3)
        return;
    if (PTE_FLAGS(pgdir))
        panic("vm_free: invalid pgdir\n");
    if (level == 0) {
        kfree((char *)pgdir);
        return;
    }
    for (uint64_t i = 0; i < ENTRYSZ; i++) {
        if (pgdir[i] & PTE_P) {
            uint64_t *v = (uint64_t *)P2V(PTE_ADDR(pgdir[i]));
            vm_free(v, level - 1);
        }
    }
    kfree((char *)pgdir);
}
