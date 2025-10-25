#ifndef MM_STATS_H
#define MM_STATS_H

#include <stdint.h>

#define MM_NAME_MAX            16
#define MM_MAX_ORDER           20
#define MM_MAX_SIMPLE_BLOCKS   32

typedef struct mm_order_info {
    uint32_t order;
    uint64_t block_size;
    uint64_t free_count;
} mm_order_info_t;

typedef struct mm_block_info {
    uint64_t addr;
    uint64_t size;
} mm_block_info_t;

typedef struct mm_stats {
    char     mm_name[MM_NAME_MAX];
    uint64_t heap_total;
    uint64_t used_bytes;
    uint64_t free_bytes;
    uint64_t free_blocks;
    uint64_t largest_free;
    uint8_t  has_buddy;
    uint8_t  max_order;
    mm_order_info_t orders[MM_MAX_ORDER];
    uint64_t heap_base;
    uint64_t heap_end;
    uint32_t freelist_count;
    uint32_t freelist_truncated;
    mm_block_info_t freelist[MM_MAX_SIMPLE_BLOCKS];
} mm_stats_t;

#endif /* MM_STATS_H */
