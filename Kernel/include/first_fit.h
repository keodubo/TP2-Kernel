#ifndef _FIRST_FIT_H_
#define _FIRST_FIT_H_

#include "memory_manager.h"
#include "mm_stats.h"

// Estructura de un bloque de memoria para First Fit
typedef struct memory_block {
    size_t size;           // Tamaño del bloque (sin incluir header)
    int is_free;           // 1 si esta libre, 0 si esta ocupado
    struct memory_block* next;  // Puntero al siguiente bloque
    uint32_t magic;        // Número mágico para detectar corrupcion
} memory_block_t;

// Magic number para detectar corrupcion
#define BLOCK_MAGIC 0xDEADBEEF
#define FREE_MAGIC  0xFEEDFACE

// Tamaño minimo de bloque (para evitar fragmentacion excesiva)
#define MIN_BLOCK_SIZE 16

// Funciones especificas del First Fit
void first_fit_init(void* start_addr, size_t total_size);
void* first_fit_malloc(size_t size);
void first_fit_free(void* ptr);
void first_fit_get_info(memory_info_t* info);
void first_fit_debug_print();
int first_fit_check_integrity();
void first_fit_collect_stats(mm_stats_t *stats);

#endif // _FIRST_FIT_H_
