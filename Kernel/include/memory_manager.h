#ifndef _MEMORY_MANAGER_H_
#define _MEMORY_MANAGER_H_

#include <stdint.h>
#include <stddef.h>

// Estructura para informacion de memoria
typedef struct {
    uint64_t total_memory;
    uint64_t used_memory;
    uint64_t free_memory;
    uint64_t allocated_blocks;
    uint64_t free_blocks;
} memory_info_t;

// Interfaz comun para ambos memory managers
void mm_init(void* start_addr, size_t total_size);
void* mm_malloc(size_t size);
void mm_free(void* ptr);
void mm_get_info(memory_info_t* info);
void mm_debug_print();

// Para debugging y testing
int mm_check_integrity();
void mm_print_blocks();

#endif // _MEMORY_MANAGER_H_