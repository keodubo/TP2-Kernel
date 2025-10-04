#include "memory_manager.h"
#include "first_fit.h"
#include <lib.h>

// Seleccion del memory manager en tiempo de compilación
#ifdef USE_BUDDY_SYSTEM
    // #include "buddy_system.h"  // Falta implementar
    #error "Buddy system not implemented yet"
#else
    // Por defecto usar First Fit
    #define USE_FIRST_FIT
#endif

// Wrappers que redirigen a la implementacion especifica
void mm_init(void* start_addr, size_t total_size) {
#ifdef USE_FIRST_FIT
    first_fit_init(start_addr, total_size);
#elif defined(USE_BUDDY_SYSTEM)
    // buddy_init(start_addr, total_size);
#endif
}

void* mm_malloc(size_t size) {
#ifdef USE_FIRST_FIT
    return first_fit_malloc(size);
#elif defined(USE_BUDDY_SYSTEM)
    // return buddy_malloc(size);
    return NULL;
#endif
}

void mm_free(void* ptr) {
    if (ptr == NULL) return;
    
#ifdef USE_FIRST_FIT
    first_fit_free(ptr);
#elif defined(USE_BUDDY_SYSTEM)
    // buddy_free(ptr);
#endif
}

void mm_get_info(memory_info_t* info) {
    if (info == NULL) return;
    
#ifdef USE_FIRST_FIT
    first_fit_get_info(info);
#elif defined(USE_BUDDY_SYSTEM)
    // buddy_get_info(info);
#endif
}

void mm_debug_print() {
#ifdef USE_FIRST_FIT
    first_fit_debug_print();
#elif defined(USE_BUDDY_SYSTEM)
    // buddy_debug_print();
#endif
}

int mm_check_integrity() {
#ifdef USE_FIRST_FIT
    return first_fit_check_integrity();
#elif defined(USE_BUDDY_SYSTEM)
    // return buddy_check_integrity();
    return 0;
#endif
}