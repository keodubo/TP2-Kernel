#include "buddy_system.h"
#include "naiveConsole.h"
#include <lib.h>

// Estructura de nodo libre en las listas de bloques
typedef struct free_node {
    struct free_node* next;
} free_node_t;

// Variables globales del Buddy System
static free_node_t* free_lists[BUDDY_MAX_ORDERS];  // Lista de bloques libres por orden
static uint8_t* order_map = NULL;                  // Mapa que indica el orden de cada bloque
static uint8_t* used_map = NULL;                   // Mapa que indica si un bloque esta usado
static void* region_base = NULL;                   // Base de la region administrada
static void* region_end = NULL;                    // Fin de la region administrada
static unsigned max_order = 0;                     // Orden maximo segun el tamaño del heap
static int initialized = 0;

// Archivo: buddy_system.c
// Propósito: Implementación del allocador Buddy System
// Resumen: Mantiene listas por orden, mapas de uso y funciones para
//          alloc/free/merge usando la técnica buddy.

// Estadisticas
static uint64_t stat_total = 0;
static uint64_t stat_used = 0;
static uint64_t stat_alloc_blocks = 0;
static uint64_t stat_free_blocks = 0;

// Funciones auxiliares
static inline void* align_up_ptr(void* ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    return (void*)((addr + alignment - 1) & ~(alignment - 1));
}

static inline void* align_down_ptr(void* ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    return (void*)(addr & ~(alignment - 1));
}

static inline size_t ptr_to_index(void* ptr) {
    return ((uintptr_t)ptr - (uintptr_t)region_base) >> BUDDY_PAGE_SHIFT;
}

static inline void* index_to_ptr(size_t idx) {
    return (void*)((uintptr_t)region_base + (idx << BUDDY_PAGE_SHIFT));
}

// Calcula el orden necesario para un tamaño dado
static unsigned size_to_order(size_t size) {
    if (size == 0) return BUDDY_MIN_ORDER;
    
    unsigned order = BUDDY_MIN_ORDER;
    size_t block_size = BUDDY_PAGE_SIZE;
    
    while (block_size < size && order < max_order) {
        block_size <<= 1;
        order++;
    }
    
    return order;
}

// Calcula el indice del buddy de un bloque dado
static inline size_t buddy_index(size_t idx, unsigned order) {
    size_t block_size = (1UL << (order - BUDDY_MIN_ORDER));
    return idx ^ block_size;
}

// Agrega un bloque a la lista de libres de su orden
static void add_to_free_list(size_t idx, unsigned order) {
    void* block_ptr = index_to_ptr(idx);
    free_node_t* node = (free_node_t*)block_ptr;
    node->next = free_lists[order];
    free_lists[order] = node;
    stat_free_blocks++;
}

// Remueve un bloque de la lista de libres de su orden
static int remove_from_free_list(size_t idx, unsigned order) {
    void* target = index_to_ptr(idx);
    free_node_t** current = &free_lists[order];
    
    while (*current != NULL) {
        if (*current == target) {
            *current = (*current)->next;
            stat_free_blocks--;
            return 1;
        }
        current = &(*current)->next;
    }
    
    return 0;
}

void buddy_init(void* start_addr, size_t total_size) {
    if (start_addr == NULL || total_size < BUDDY_PAGE_SIZE * 2) {
        return;
    }
    
    // Alinear la region a BUDDY_PAGE_SIZE
    void* aligned_start = align_up_ptr(start_addr, BUDDY_PAGE_SIZE);
    void* aligned_end = align_down_ptr((void*)((uintptr_t)start_addr + total_size), BUDDY_PAGE_SIZE);
    
    if (aligned_end <= aligned_start) {
        return;
    }
    
    size_t aligned_size = (uintptr_t)aligned_end - (uintptr_t)aligned_start;
    size_t num_pages = aligned_size >> BUDDY_PAGE_SHIFT;
    
    // Calcular el orden maximo
    max_order = BUDDY_MIN_ORDER;
    size_t test_size = BUDDY_PAGE_SIZE;
    while (test_size < aligned_size && max_order < BUDDY_MAX_ORDERS - 1) {
        test_size <<= 1;
        max_order++;
    }
    
    // Reservar espacio para los mapas al inicio del heap
    size_t metadata_size = num_pages * 2; // order_map + used_map
    metadata_size = (metadata_size + BUDDY_PAGE_SIZE - 1) & ~(BUDDY_PAGE_SIZE - 1); // Alinear a pagina
    
    if (metadata_size >= aligned_size / 2) {
        return; // No hay suficiente espacio
    }
    
    // Inicializar mapas
    order_map = (uint8_t*)aligned_start;
    used_map = order_map + num_pages;
    
    // Limpiar mapas
    for (size_t i = 0; i < num_pages; i++) {
        order_map[i] = 0xFF; // No es head de ningun bloque
        used_map[i] = 0;     // Libre
    }
    
    // La region disponible empieza despues de los metadatos
    region_base = (void*)((uintptr_t)aligned_start + metadata_size);
    region_end = aligned_end;
    
    // Inicializar listas de libres
    for (unsigned i = 0; i < BUDDY_MAX_ORDERS; i++) {
        free_lists[i] = NULL;
    }
    
    // Crear bloques iniciales del maximo orden posible
    size_t available_size = (uintptr_t)region_end - (uintptr_t)region_base;
    size_t current_offset = 0;
    
    while (current_offset < available_size) {
        size_t remaining = available_size - current_offset;
        unsigned order = size_to_order(remaining);
        
        if (order > max_order) {
            order = max_order;
        }
        
        size_t block_size = 1UL << order;
        if (block_size > remaining) {
            order--;
            block_size = 1UL << order;
        }
        
        size_t idx = ((uintptr_t)region_base + current_offset - (uintptr_t)aligned_start) >> BUDDY_PAGE_SHIFT;
        order_map[idx] = order;
        used_map[idx] = 0;
        add_to_free_list(idx, order);
        
        current_offset += block_size;
    }
    
    stat_total = available_size;
    stat_used = 0;
    stat_alloc_blocks = 0;
    initialized = 1;
}

void* buddy_alloc(size_t size) {
    if (!initialized || size == 0) {
        return NULL;
    }
    
    // Calcular el orden necesario
    unsigned order = size_to_order(size);
    
    if (order > max_order) {
        return NULL; // Pedido muy grande
    }
    
    // Buscar un bloque libre del orden necesario o mayor
    unsigned current_order = order;
    while (current_order <= max_order && free_lists[current_order] == NULL) {
        current_order++;
    }
    
    if (current_order > max_order) {
        return NULL; // No hay bloques disponibles
    }
    
    // Tomar el primer bloque de la lista
    free_node_t* block = free_lists[current_order];
    free_lists[current_order] = block->next;
    stat_free_blocks--;
    
    size_t idx = ptr_to_index(block);
    
    // Dividir el bloque hasta llegar al orden necesario
    while (current_order > order) {
        current_order--;
        size_t buddy_idx = buddy_index(idx, current_order + 1);
        
        order_map[buddy_idx] = current_order;
        used_map[buddy_idx] = 0;
        add_to_free_list(buddy_idx, current_order);
    }
    
    // Marcar el bloque como usado
    order_map[idx] = order;
    used_map[idx] = 1;
    
    size_t block_size = 1UL << order;
    stat_used += block_size;
    stat_alloc_blocks++;
    
    return (void*)block;
}

void buddy_free(void* ptr) {
    if (!initialized || ptr == NULL) {
        return;
    }
    
    if (ptr < region_base || ptr >= region_end) {
        return; // Puntero fuera de rango
    }
    
    size_t idx = ptr_to_index(ptr);
    
    if (order_map[idx] == 0xFF || used_map[idx] == 0) {
        return; // Bloque no valido o ya liberado
    }
    
    unsigned order = order_map[idx];
    size_t block_size = 1UL << order;
    
    // Marcar como libre
    used_map[idx] = 0;
    stat_used -= block_size;
    stat_alloc_blocks--;
    
    // Intentar fusionar con el buddy
    while (order < max_order) {
        size_t buddy_idx = buddy_index(idx, order);
        
        // Verificar si el buddy existe y esta libre
        if (buddy_idx >= ((uintptr_t)region_end - (uintptr_t)region_base) >> BUDDY_PAGE_SHIFT) {
            break;
        }
        
        if (order_map[buddy_idx] != order || used_map[buddy_idx] != 0) {
            break; // Buddy no disponible para fusionar
        }
        
        // Remover el buddy de su lista
        if (!remove_from_free_list(buddy_idx, order)) {
            break;
        }
        
        // Fusionar: el bloque resultante tiene el indice menor
        if (buddy_idx < idx) {
            idx = buddy_idx;
        }
        
        order++;
        order_map[idx] = order;
    }
    
    // Agregar el bloque fusionado a la lista de libres
    add_to_free_list(idx, order);
}

void buddy_get_info(memory_info_t* info) {
    if (!initialized || info == NULL) {
        return;
    }
    
    info->total_memory = stat_total;
    info->used_memory = stat_used;
    info->free_memory = stat_total - stat_used;
    info->allocated_blocks = stat_alloc_blocks;
    info->free_blocks = stat_free_blocks;
}

void buddy_debug_print(void) {
    if (!initialized) {
        ncPrint("Buddy System no inicializado\n");
        return;
    }
    
    ncPrint("=== Buddy System Debug ===\n");
    ncPrint("Total: ");
    ncPrintDec(stat_total);
    ncPrint(" bytes\n");
    
    ncPrint("Usado: ");
    ncPrintDec(stat_used);
    ncPrint(" bytes\n");
    
    ncPrint("Libre: ");
    ncPrintDec(stat_total - stat_used);
    ncPrint(" bytes\n");
    
    ncPrint("Bloques asignados: ");
    ncPrintDec(stat_alloc_blocks);
    ncNewline();
    
    ncPrint("Bloques libres: ");
    ncPrintDec(stat_free_blocks);
    ncNewline();
}

int buddy_check_integrity(void) {
    if (!initialized) {
        return 0;
    }
    
    // Verificacion basica: contar bloques en listas libres
    uint64_t counted_free = 0;
    for (unsigned order = BUDDY_MIN_ORDER; order <= max_order; order++) {
        free_node_t* current = free_lists[order];
        while (current != NULL) {
            counted_free++;
            current = current->next;
        }
    }
    
    return (counted_free == stat_free_blocks) ? 1 : 0;
}
