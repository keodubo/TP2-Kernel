#include "first_fit.h"
#include "naiveConsole.h"
#include <lib.h>

// Variables globales para el heap
static memory_block_t* heap_start = NULL;
static void* heap_end = NULL;
static size_t total_heap_size = 0;
static int initialized = 0;

// Estadisticas
static uint64_t total_allocations = 0;
static uint64_t total_frees = 0;
static uint64_t current_allocated_blocks = 0;

void first_fit_init(void* start_addr, size_t total_size) {
    if (start_addr == NULL || total_size < sizeof(memory_block_t)) {
        return;
    }
    
    // Alinear la direccion de inicio a 8 bytes
    uintptr_t aligned_start = ((uintptr_t)start_addr + 7) & ~7;
    size_t adjusted_size = total_size - (aligned_start - (uintptr_t)start_addr);
    
    heap_start = (memory_block_t*)aligned_start;
    heap_end = (void*)(aligned_start + adjusted_size);
    total_heap_size = adjusted_size;
    
    // Inicializar el primer bloque libre (todo el heap)
    heap_start->size = adjusted_size - sizeof(memory_block_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
    heap_start->magic = FREE_MAGIC;
    
    initialized = 1;
    
    // Limpiar estadísticas
    total_allocations = 0;
    total_frees = 0;
    current_allocated_blocks = 0;
}

static memory_block_t* find_free_block(size_t size) {
    memory_block_t* current = heap_start;
    
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return NULL; // No se encontro bloque libre suficiente
}

static void split_block(memory_block_t* block, size_t size) {
    if (block == NULL || !block->is_free) {
        return;
    }
    
    // Solo dividir si el bloque restante es lo suficientemente grande
    size_t remaining_size = block->size - size;
    if (remaining_size < sizeof(memory_block_t) + MIN_BLOCK_SIZE) {
        return; // No dividir, desperdiciaría muy poco espacio
    }
    
    // Crear nuevo bloque libre con el espacio restante
    memory_block_t* new_block = (memory_block_t*)((char*)block + sizeof(memory_block_t) + size);
    new_block->size = remaining_size - sizeof(memory_block_t);
    new_block->is_free = 1;
    new_block->next = block->next;
    new_block->magic = FREE_MAGIC;
    
    // Actualizar el bloque actual
    block->size = size;
    block->next = new_block;
}

static void coalesce_blocks() {
    memory_block_t* current = heap_start;
    
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            // Verificar que los bloques sean adyacentes
            void* current_end = (char*)current + sizeof(memory_block_t) + current->size;
            if (current_end == current->next) {
                // Fusionar bloques
                memory_block_t* next_block = current->next;
                current->size += sizeof(memory_block_t) + next_block->size;
                current->next = next_block->next;
                // No avanzar current para verificar si se puede fusionar con el siguiente
                continue;
            }
        }
        current = current->next;
    }
}

static memory_block_t* get_block_header(void* ptr) {
    if (ptr == NULL) {
        return NULL;
    }
    
    // El header esta justo antes del puntero de datos
    return (memory_block_t*)((char*)ptr - sizeof(memory_block_t));
}

void* first_fit_malloc(size_t size) {
    if (!initialized || size == 0) {
        return NULL;
    }
    
    // Alinear el tamaño a 8 bytes
    size = (size + 7) & ~7;
    
    // Asegurar tamaño minimo
    if (size < MIN_BLOCK_SIZE) {
        size = MIN_BLOCK_SIZE;
    }
    
    // Buscar un bloque libre
    memory_block_t* block = find_free_block(size);
    if (block == NULL) {
        return NULL; // No hay memoria suficiente
    }
    
    // Dividir el bloque si es necesario
    split_block(block, size);
    
    // Marcar el bloque como ocupado
    block->is_free = 0;
    block->magic = BLOCK_MAGIC;
    
    // Actualizar estadisticas
    total_allocations++;
    current_allocated_blocks++;
    
    // Retornar puntero a los datos (despues del header)
    return (char*)block + sizeof(memory_block_t);
}

void first_fit_free(void* ptr) {
    if (!initialized || ptr == NULL) {
        return;
    }
    
    memory_block_t* block = get_block_header(ptr);
    
    // Verificaciones de integridad
    if (block == NULL || 
        block->magic != BLOCK_MAGIC ||
        block->is_free ||
        (char*)block < (char*)heap_start ||
        (char*)block >= (char*)heap_end) {
        return; // Puntero invalido o ya liberado
    }
    
    // Marcar como libre
    block->is_free = 1;
    block->magic = FREE_MAGIC;
    
    // Actualizar estadisticas
    total_frees++;
    current_allocated_blocks--;
    
    // Limpiar los datos
    memset(ptr, 0, block->size);
    
    // Fusionar bloques adyacentes libres
    coalesce_blocks();
}

void first_fit_get_info(memory_info_t* info) {
    if (!initialized || info == NULL) {
        return;
    }
    
    info->total_memory = total_heap_size;
    info->used_memory = 0;
    info->free_memory = 0;
    info->allocated_blocks = 0;
    info->free_blocks = 0;
    
    memory_block_t* current = heap_start;
    while (current != NULL) {
        if (current->is_free) {
            info->free_memory += current->size;
            info->free_blocks++;
        } else {
            info->used_memory += current->size;
            info->allocated_blocks++;
        }
        // Agregar overhead del header
        info->used_memory += sizeof(memory_block_t);
        current = current->next;
    }
}

void first_fit_debug_print() {
    if (!initialized) {
        ncPrint("Memory manager not initialized\n");
        return;
    }
    
    ncPrint("=== First Fit Memory Manager Debug ===\n");
    ncPrint("Heap start: 0x");
    ncPrintHex((uint64_t)heap_start);
    ncPrint("\nHeap end: 0x");
    ncPrintHex((uint64_t)heap_end);
    ncPrint("\nTotal size: ");
    ncPrintDec(total_heap_size);
    ncPrint(" bytes\n");
    
    ncPrint("Total allocations: ");
    ncPrintDec(total_allocations);
    ncPrint("\nTotal frees: ");
    ncPrintDec(total_frees);
    ncPrint("\nCurrent allocated blocks: ");
    ncPrintDec(current_allocated_blocks);
    ncPrint("\n\n");
    
    memory_info_t info;
    first_fit_get_info(&info);
    
    ncPrint("Memory usage:\n");
    ncPrint("  Total: ");
    ncPrintDec(info.total_memory);
    ncPrint(" bytes\n");
    ncPrint("  Used: ");
    ncPrintDec(info.used_memory);
    ncPrint(" bytes\n");
    ncPrint("  Free: ");
    ncPrintDec(info.free_memory);
    ncPrint(" bytes\n");
    ncPrint("  Allocated blocks: ");
    ncPrintDec(info.allocated_blocks);
    ncPrint("\n");
    ncPrint("  Free blocks: ");
    ncPrintDec(info.free_blocks);
    ncPrint("\n\n");
    
    // Mostrar lista de bloques
    ncPrint("Block list:\n");
    memory_block_t* current = heap_start;
    int block_num = 0;
    
    while (current != NULL && block_num < 20) { // Limitar salida
        ncPrint("Block ");
        ncPrintDec(block_num);
        ncPrint(": addr=0x");
        ncPrintHex((uint64_t)current);
        ncPrint(", size=");
        ncPrintDec(current->size);
        ncPrint(", ");
        ncPrint(current->is_free ? "FREE" : "USED");
        ncPrint(", magic=0x");
        ncPrintHex(current->magic);
        ncPrint("\n");
        
        current = current->next;
        block_num++;
    }
    
    if (current != NULL) {
        ncPrint("... (more blocks)\n");
    }
    
    ncPrint("=====================================\n");
}

int first_fit_check_integrity() {
    if (!initialized) {
        return 0;
    }
    
    memory_block_t* current = heap_start;
    int errors = 0;
    
    while (current != NULL) {
        // Verificar magic number
        uint32_t expected_magic = current->is_free ? FREE_MAGIC : BLOCK_MAGIC;
        if (current->magic != expected_magic) {
            errors++;
        }
        
        // Verificar que el bloque este dentro del heap
        if ((char*)current < (char*)heap_start || 
            (char*)current >= (char*)heap_end) {
            errors++;
            break; // No seguir si estamos fuera del heap
        }
        
        // Verificar que el next pointer sea valido
        if (current->next != NULL &&
            ((char*)current->next < (char*)heap_start ||
             (char*)current->next >= (char*)heap_end)) {
            errors++;
        }
        
        current = current->next;
    }
    
    return errors == 0;
}