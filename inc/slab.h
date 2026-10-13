#ifndef INC_SLAB_H
#define INC_SLAB_H

#include <types.h>
#include <spinlock.h>

/**
 * @ingroup slab
 * @def MAX_SLAB_NAME
 * @brief スラブ名の長さ
 */
#define MAX_SLAB_NAME   32
/**
 * @ingroup slab
 * @def SLAB_FREE_END
 * @brief フリーオブジェクトリストの終了マーク
 */
#define SLAB_FREE_END   0xffffffff

/**
 * @ingroup slab
 * @def SLAB_HEADER_SIZE
 * @brief スラブヘッダー長
 */
#define SLAB_HEADER_SIZE (sizeof(struct slab_header) + sizeof(uint32_t))

/**
 * @ingroup slab
 * @struct slab_header
 * @brief スラブヘッダー構造体.
 */
struct slab_header {
    struct slab_header *next;   /**< 次のスラブヘッダーへのポインタ */
    uint32_t *free;             /**< フリーオブジェクト番号リストへのポインタ */
    uint8_t *object;            /**< オブジェクトリストへのポインタ */
};

/**
 * @ingroup slab
 * @struct slab_cache
 * @brief スラブキャッシュ構造体.
 */
struct slab_cache {
    char name[MAX_SLAB_NAME];   /**< キャッシュ名 */

    struct slab_cache *next;    /**< 次のスラブキャッシュへのポインタ */

    uint32_t slab_size;         /**< スラブのサイズ（page * 2^order） */
    uint32_t object_size;       /**< オブジェクトのサイズ（4バイト切り上げ） */
    uint32_t alignment;         /**< アライメント. 不要な場合は0 */

    struct slab_header *slabs_full;     /**< 全使用済みリスト */
    struct slab_header *slabs_partial;  /**< 一部使用済みリスト */
    struct spinlock lock;
};

void            slab_cache_init(void);
struct slab_cache *slab_cache_create(const char *name, size_t size, uint32_t alignment);
void            slab_cache_destroy(struct slab_cache *cache);
void *          slab_cache_alloc(struct slab_cache *cache);
void            slab_cache_free(struct slab_cache *cache, void *obj);

#endif
