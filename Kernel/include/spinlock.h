/**
 * Spinlocks - Primitivas de Sincronización del Kernel
 *
 * Los spinlocks son mecanismos de exclusión mutua de bajo nivel que protegen
 * secciones críticas del código mediante busy-waiting activo.
 *
 * Características:
 * ----------------
 * - Busy-wait: El CPU espera activamente hasta obtener el lock
 * - Atómico: Usa instrucciones atómicas del hardware (__sync_lock_test_and_set)
 * - Corta duración: Solo para secciones críticas MUY cortas (microsegundos)
 * - No apropiado para esperas largas (usar semáforos en ese caso)
 *
 * Variantes disponibles:
 * ----------------------
 * 1. spinlock_lock_irqsave() / spinlock_unlock_irqrestore():
 *    - Guarda el estado de interrupciones y las deshabilita
 *    - Restaura el estado original al liberar
 *    - USO: Secciones críticas que pueden ser interrumpidas por IRQs
 *    - EJEMPLO: Acceso a estructuras compartidas con interrupt handlers
 *
 * 2. spinlock_lock() / spinlock_unlock():
 *    - Adquiere/libera el lock SIN modificar interrupciones
 *    - USO: Código que ya está en contexto de interrupciones deshabilitadas
 *    - PRECAUCIÓN: Puede causar deadlock si interrumpe código con el mismo lock
 *
 * Reglas de uso:
 * --------------
 * - Mantener secciones críticas LO MÁS CORTAS posible
 * - NUNCA llamar a funciones bloqueantes dentro de un spinlock
 * - NUNCA hacer I/O o asignación de memoria dentro del lock
 * - Siempre liberar en el mismo orden que se adquiere (evitar deadlocks)
 * - Usar irqsave/irqrestore cuando se comparte con interrupt handlers
 *
 * Ejemplo de uso correcto:
 * ------------------------
 * spinlock_t my_lock;
 * spinlock_init(&my_lock);
 *
 * uint64_t flags = spinlock_lock_irqsave(&my_lock);
 * // Sección crítica MUY corta
 * shared_data++;
 * spinlock_unlock_irqrestore(&my_lock, flags);
 */

#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

#include <stdint.h>
#include "interrupts.h"

/**
 * Estructura del spinlock.
 * value == 0: desbloqueado
 * value == 1: bloqueado
 */
typedef struct {
    volatile int value;
} spinlock_t;

/**
 * Inicializa un spinlock en estado desbloqueado.
 * Debe llamarse antes de usar el spinlock.
 */
static inline void spinlock_init(spinlock_t *lock) {
    if (lock != NULL) {
        lock->value = 0;
    }
}

/**
 * Adquiere el spinlock guardando y deshabilitando interrupciones.
 * Retorna el estado previo de interrupciones para restaurar luego.
 *
 * USO: Proteger datos compartidos que también se acceden desde IRQ handlers.
 * IMPORTANTE: Usar siempre con spinlock_unlock_irqrestore().
 */
static inline uint64_t spinlock_lock_irqsave(spinlock_t *lock) {
    uint64_t flags;
    __asm__ volatile("pushfq\n\tpop %0" : "=r"(flags));  // Guardar RFLAGS
    _cli();  // Deshabilitar interrupciones
    // Busy-wait hasta obtener el lock (test-and-set atómico)
    while (__sync_lock_test_and_set(&lock->value, 1)) {
        __asm__ volatile("pause");  // Hint al CPU para optimizar busy-wait
    }
    return flags;
}

/**
 * Libera el spinlock y restaura el estado previo de interrupciones.
 *
 * @param flags: Valor retornado por spinlock_lock_irqsave()
 */
static inline void spinlock_unlock_irqrestore(spinlock_t *lock, uint64_t flags) {
    __sync_lock_release(&lock->value);  // Liberar el lock atómicamente
    // Restaurar interrupciones solo si estaban habilitadas antes
    if (flags & (1ULL << 9)) {  // Bit 9 de RFLAGS = Interrupt Enable Flag
        _sti();
    }
}

/**
 * Adquiere el spinlock SIN modificar el estado de interrupciones.
 *
 * USO: Cuando ya se ejecuta con interrupciones deshabilitadas.
 * PRECAUCIÓN: No usar si el lock puede ser tomado desde un IRQ handler.
 */
static inline void spinlock_lock(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->value, 1)) {
        __asm__ volatile("pause");
    }
}

/**
 * Libera el spinlock SIN modificar el estado de interrupciones.
 */
static inline void spinlock_unlock(spinlock_t *lock) {
    __sync_lock_release(&lock->value);
}

#endif /* KERNEL_SPINLOCK_H */
