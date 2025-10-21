#include "memory_manager.h"
#include "first_fit.h"
#include "buddy_system.h"
#include "naiveConsole.h"
#include <lib.h>

// Seleccion del memory manager en tiempo de compilacion
#ifdef USE_BUDDY_SYSTEM
    // Usar Buddy System
#else
    // Por defecto usar First Fit
    #define USE_FIRST_FIT
#endif

    // Archivo: memory_manager.c
    // Propósito: Envolver las implementaciones de gestión de memoria disponibles
    // Resumen: Selecciona en tiempo de compilación entre Buddy System y First Fit
    //          y expone una API uniforme: mm_init, mm_malloc, mm_free, mm_get_info.

// Wrappers que redirigen a la implementacion especifica
void mm_init(void* start_addr, size_t total_size) {
#ifdef USE_BUDDY_SYSTEM
    buddy_init(start_addr, total_size);
#else
    first_fit_init(start_addr, total_size);
#endif
}

void* mm_malloc(size_t size) {
#ifdef USE_BUDDY_SYSTEM
    return buddy_alloc(size);
#else
    return first_fit_malloc(size);
#endif
}

void mm_free(void* ptr) {
    if (ptr == NULL) return;
    
#ifdef USE_BUDDY_SYSTEM
    buddy_free(ptr);
#else
    first_fit_free(ptr);
#endif
}

void mm_get_info(memory_info_t* info) {
    if (info == NULL) return;
    
#ifdef USE_BUDDY_SYSTEM
    buddy_get_info(info);
#else
    first_fit_get_info(info);
#endif
}

void mm_debug_print() {
#ifdef USE_BUDDY_SYSTEM
    buddy_debug_print();
#else
    first_fit_debug_print();
#endif
}

int mm_check_integrity() {
#ifdef USE_BUDDY_SYSTEM
    return buddy_check_integrity();
#else
    return first_fit_check_integrity();
#endif
}