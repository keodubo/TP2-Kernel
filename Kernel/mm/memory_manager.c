#include "memory_manager.h"
#include "first_fit.h"
#include "buddy_system.h"
#include "naiveConsole.h"
#include <lib.h>

// Wrapper que selecciona el memory manager en tiempo de compilacion
// Por defecto usa First Fit, con "make buddy" usa Buddy System
// Solo UN algoritmo puede estar activo a la vez

#ifdef USE_BUDDY_SYSTEM
    // Usar Buddy System
#else
    // Por defecto usar First Fit
    #define USE_FIRST_FIT
#endif
void mm_init(void* start_addr, size_t total_size) {
#ifdef USE_BUDDY_SYSTEM
    buddy_init(start_addr, total_size);
#else
    first_fit_init(start_addr, total_size);
#endif
}

// Asigna un bloque de memoria del tamaño solicitado
void* mm_malloc(size_t size) {
#ifdef USE_BUDDY_SYSTEM
    return buddy_alloc(size);
#else
    return first_fit_malloc(size);
#endif
}

// Libera un bloque de memoria previamente asignado
void mm_free(void* ptr) {
    if (ptr == NULL) return;
    
#ifdef USE_BUDDY_SYSTEM
    buddy_free(ptr);
#else
    first_fit_free(ptr);
#endif
}

// Obtiene estadisticas del estado actual de la memoria
void mm_get_info(memory_info_t* info) {
    if (info == NULL) return;
    
#ifdef USE_BUDDY_SYSTEM
    buddy_get_info(info);
#else
    first_fit_get_info(info);
#endif
}

// Imprime informacion de debugging sobre el estado del heap
void mm_debug_print() {
#ifdef USE_BUDDY_SYSTEM
    buddy_debug_print();
#else
    first_fit_debug_print();
#endif
}

// Verifica la integridad de las estructuras de memoria
// Retorna 1 si todo esta OK, 0 si hay corrupcion
int mm_check_integrity() {
#ifdef USE_BUDDY_SYSTEM
    return buddy_check_integrity();
#else
    return first_fit_check_integrity();
#endif
}

// Recolecta estadisticas detalladas para userland
// Usada por el comando 'mem' para mostrar estado del heap
void mm_collect_stats(mm_stats_t *stats) {
    if (stats == NULL) {
        return;
    }
#ifdef USE_BUDDY_SYSTEM
    buddy_collect_stats(stats);
#else
    first_fit_collect_stats(stats);
#endif
}
