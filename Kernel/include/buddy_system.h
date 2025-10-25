#ifndef _BUDDY_SYSTEM_H_
#define _BUDDY_SYSTEM_H_

#include <stdint.h>
#include <stddef.h>
#include "memory_manager.h"

// Configuracion del Buddy System
#define BUDDY_PAGE_SHIFT   12
#define BUDDY_PAGE_SIZE    (1 << BUDDY_PAGE_SHIFT)
#define BUDDY_MIN_ORDER    BUDDY_PAGE_SHIFT
#define BUDDY_MAX_ORDERS   20

// Funciones del Buddy System
void buddy_init(void* start_addr, size_t total_size);
void* buddy_alloc(size_t size);
void buddy_free(void* ptr);
void buddy_get_info(memory_info_t* info);
void buddy_debug_print(void);
int buddy_check_integrity(void);
void buddy_collect_stats(mm_stats_t *stats);

#endif
