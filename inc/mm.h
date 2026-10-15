/*
Copyright 2014 Akira Midorikawa

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
/**
 * @file mm.h
 */
#ifndef INC_MM_H
#define INC_MM_H

#include <list.h>
#include <mmu.h>

#define PGSIZE  4096

/**
 * @ingroup mm
 * @def PAGE_START
 * @brief ページ領域の先頭（仮想）アドレス
 *        pages[0:PAGE_NUM] | page0, page1, ...
 */
#define PAGE_START ((char*)(0x4a400000))

/**
 * @ingroup mm
 * @def PAGE_NUM
 * @brief 総ページ数.
 *        ページに使用できるのは実機で 0x35c00 (220,160) * 0x1000 (4096) = 860MB
 */
#define PAGE_NUM  0x35c00

/**
 * @ingroup mm
 * @def PAGE_MAX_ORDER
 * @brief ページブロックの最大オーダ数.
 *        最大ページブロックは 2^10 (1024) * 4KB = 4MB.
 */
#define PAGE_MAX_ORDER 10
/**
 * @ingroup mm
 * @def PAGE_MAX_DEPTH
 * @brief 最大のページ深さ.
 */
#define PAGE_MAX_DEPTH (PAGE_MAX_ORDER + 1)
/**
 * @ingroup mm
 * @def PF_FREE_LIST
 * @brief ページフラグ: 空きリスト
 */
#define PF_FREE_LIST  (1 << 0)
/**
 * @ingroup mm
 * @def PF_FIRST_PAGE
 * @brief ページフラグ: 2分割の前方ブロック
 */
#define PF_FIRST_PAGE (1 << 1)
/**
 * @ingroup mm
 * @def _page_cleanup_
 * @brief ページクリーンアップ関数
 */
#define _page_cleanup_ _cleanup_(page_cleanup)

/**
 * @ingroup mm
 * @var page_index
 * @brief pages配列のインデックス.
 */
typedef uint64_t page_index;

/**
 * @ingroup mm
 * @struct page
 * @brief ページ構造体.
 */
struct page {
    page_index  index;      /**< ページインデックス (0 : PAGE_NUM - 1) */
    uint16_t    flags;      /**< フラグ */
    uint16_t    order;      /**< ページブロックの大きさ (2^order) */
    int         ref;        /**< 参照カウント */
    struct page* next;      /**< 次のページ構造体へのポインタ */
};

extern unsigned paging_pages_used;

extern struct page *pages;

void            mm_init(void);
void *          page_address(const struct page *page);
struct page *   page_find_by_address(void *address);
struct page *   page_find_head(const struct page *page);
void            page_cleanup(struct page **page);

void *          kalloc(size_t num);
void            kfree(void *va);
void            inc_kmem_ref(void *va);
void            dec_kmem_ref(void *va);
int             get_kmem_ref(void *va);

void *          kmalloc(size_t nbytes);
void            kmfree(void *ap);
void *          kmrealloc(void *ptr, size_t new_size, size_t old_size);
void *          kmzalloc(size_t nbytes);

void            mm_test(void);
#endif
