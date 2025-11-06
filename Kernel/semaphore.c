#include "include/semaphore.h"
#include "include/memory_manager.h"
#include "include/interrupts.h"
#include "include/lib.h"

// Semáforos nombrados en kernel space, con hash table y colas de espera
// Permite sincronizacion entre procesos no relacionados que acuerdan un nombre
// SIN SPINLOCKS: Los procesos se bloquean realmente (no hay busy-wait)
// Wakeup FIFO, atomicidad garantizada con CLI/STI

#define SEM_HANDLE_MAX 128

static ksem_t *sem_buckets[KSEM_HASH_BUCKETS] = {0};
static volatile int sem_creation_lock = 0;  // Simple spinlock for semaphore creation

static uint32_t sem_hash(const char *name);      // Hash simple (djb2)
static uint32_t sem_name_len(const char *name);
static int sem_name_cmp(const char *a, const char *b);
static void sem_name_copy(char *dst, const char *src);
static uint64_t irq_save(void);
static void irq_restore(uint64_t flags);
static void sem_free(ksem_t *sem);

// Archivo: semaphore.c
// Propósito: Implementación de semáforos en espacio kernel
// Resumen: Tablas hash de semáforos nombrados, creación, apertura, espera
//          y señalización con manejo de colas de procesos bloqueados.

static uint32_t sem_hash(const char *name) {
    uint32_t hash = 5381;
    char c;
    while (name != NULL && (c = *name++) != '\0') {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % KSEM_HASH_BUCKETS;
}

static uint32_t sem_name_len(const char *name) {
    if (name == NULL) {
        return 0;
    }
    uint32_t len = 0;
    while (name[len] != '\0' && len < KSEM_NAME_MAX) {
        len++;
    }
    return len;
}

static int sem_name_cmp(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return (int)(unsigned char)(*a) - (int)(unsigned char)(*b);
        }
        a++;
        b++;
    }
    return (int)(unsigned char)(*a) - (int)(unsigned char)(*b);
}

static void sem_name_copy(char *dst, const char *src) {
    uint32_t i = 0;
    for (; i < KSEM_NAME_MAX - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq\n\tpop %0" : "=r"(flags));
    _cli();
    return flags;
}

static void irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) {
        _sti();
    }
}

static void sem_free(ksem_t *sem) {
    if (sem == NULL) {
        return;
    }
    sem_waiter_t *waiter = sem->wait_head;
    while (waiter != NULL) {
        sem_waiter_t *next = waiter->next;
        mm_free(waiter);
        waiter = next;
    }
    mm_free(sem);
}

// Obtiene (o crea) un semáforo nominal y ajusta su refcount
int ksem_open(const char *name, unsigned int init, ksem_t **out) {
    if (name == NULL || out == NULL) {
        return -1;
    }

    uint32_t len = sem_name_len(name);
    if (len == 0 || len >= KSEM_NAME_MAX) {
        return -1;
    }

    // Acquire global creation lock to prevent races during semaphore creation
    // This ensures only one process can check-allocate-insert at a time
    while (__sync_lock_test_and_set(&sem_creation_lock, 1)) {
        // Spin until we get the lock
        __asm__ volatile("pause");
    }

    ksem_t *sem = NULL;
    uint64_t flags = irq_save();

    uint32_t bucket = sem_hash(name);
    ksem_t *cursor = sem_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && sem_name_cmp(cursor->name, name) == 0) {
            cursor->refcount++;
            sem = cursor;
            break;
        }
        cursor = cursor->hash_next;
    }

    irq_restore(flags);

    if (sem != NULL) {
        __sync_lock_release(&sem_creation_lock);  // Release lock before returning
        *out = sem;
        return 0;
    }

    // Semaphore doesn't exist, allocate one (still holding creation lock)
    ksem_t *new_sem = (ksem_t *)mm_malloc(sizeof(ksem_t));
    if (new_sem == NULL) {
        __sync_lock_release(&sem_creation_lock);  // Release lock before returning
        return -1;
    }
    memset(new_sem, 0, sizeof(ksem_t));
    sem_name_copy(new_sem->name, name);
    new_sem->count = init;
    new_sem->refcount = 1;
    new_sem->unlinked = false;
    new_sem->wait_head = NULL;
    new_sem->wait_tail = NULL;
    new_sem->hash_next = NULL;

    // Double-check: another process might have created it while we were allocating
    // This is now safe because we hold the creation lock
    flags = irq_save();

    cursor = sem_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && sem_name_cmp(cursor->name, name) == 0) {
            cursor->refcount++;
            irq_restore(flags);
            __sync_lock_release(&sem_creation_lock);  // Release lock before returning
            sem_free(new_sem);
            *out = cursor;
            return 0;
        }
        cursor = cursor->hash_next;
    }

    // Insert our new semaphore
    new_sem->hash_next = sem_buckets[bucket];
    sem_buckets[bucket] = new_sem;
    irq_restore(flags);

    __sync_lock_release(&sem_creation_lock);  // Release lock after insertion
    *out = new_sem;
    return 0;
}

// Decrementa el semáforo o bloquea al proceso si el contador está en cero
// SIN SPINLOCKS: bloquea el proceso realmente (sin busy-wait)
int ksem_wait(ksem_t *sem) {
    if (sem == NULL) {
        return -1;
    }

    pcb_t *current = sched_current();
    if (current == NULL) {
        return -1;
    }

    // Pre-allocate waiter node BEFORE entering critical section
    // This prevents calling mm_malloc with interrupts disabled, which can cause deadlock
    sem_waiter_t *waiter = (sem_waiter_t *)mm_malloc(sizeof(sem_waiter_t));
    if (waiter == NULL) {
        // DEBUG: mm_malloc failed
        extern void ncPrintDec(uint64_t value);
        extern void ncPrint(const char *string);
        ncPrint("[KERNEL] sem_wait: mm_malloc FAILED for PID ");
        ncPrintDec(current->pid);
        ncPrint("\n");
        return -1;
    }

    // Sección crítica: deshabilitar interrupciones para atomicidad
    uint64_t flags = irq_save();

    // Fast path: Si count > 0, simplemente decrementar y retornar
    if (sem->count > 0) {
        sem->count--;
        irq_restore(flags);
        // Free the pre-allocated waiter since we didn't need it
        mm_free(waiter);
        return 0;
    }

    // Slow path: count == 0, necesitamos bloquear el proceso
    // 1. Use the pre-allocated waiter node
    waiter->proc = current;
    waiter->next = NULL;

    // 2. Encolar al final de la cola (FIFO)
    if (sem->wait_tail == NULL) {
        sem->wait_head = waiter;
        sem->wait_tail = waiter;
    } else {
        sem->wait_tail->next = waiter;
        sem->wait_tail = waiter;
    }

    // 3. CRÍTICO: Marcar proceso como BLOCKED DENTRO de la sección crítica
    // Esto previene "lost wakeup": si ksem_post() se ejecuta ahora,
    // ya estamos marcados como BLOCKED, entonces proc_unblock() funcionará correctamente
    current->state = BLOCKED;
    current->ticks_left = 0;

    // Fin de la sección crítica
    irq_restore(flags);

    // 4. Ceder el CPU inmediatamente (bloqueo cooperativo, NO busy-wait)
    sched_force_yield();
    return 0;
}

// Incrementa el semáforo y despierta a un proceso bloqueado, si lo hay
// Usa wakeup FIFO: despierta siempre al proceso que esperó por más tiempo
int ksem_post(ksem_t *sem) {
    if (sem == NULL) {
        return -1;
    }

    sem_waiter_t *waiter = NULL;
    
    // Sección crítica: deshabilitar interrupciones
    uint64_t flags = irq_save();
    
    if (sem->wait_head != NULL) {
        // Hay procesos esperando: despertar al primero (FIFO)
        waiter = sem->wait_head;
        sem->wait_head = waiter->next;
        if (sem->wait_head == NULL) {
            sem->wait_tail = NULL;  // Cola vacía
        }
    } else {
        // No hay waiters: incrementar contador
        if (sem->count < UINT32_MAX) {
            sem->count++;
        }
    }
    irq_restore(flags);

    // Despertar el proceso FUERA de la sección crítica
    if (waiter != NULL) {
        int unblock_result = proc_unblock(waiter->proc->pid);
        if (unblock_result < 0) {
            // DEBUG: proc_unblock failed
            extern void ncPrintDec(uint64_t value);
            extern void ncPrint(const char *string);
            ncPrint("[KERNEL] sem_post: proc_unblock FAILED for PID ");
            ncPrintDec(waiter->proc->pid);
            ncPrint("\n");
        }
        mm_free(waiter);
    }

    return 0;
}

// Libera la referencia local; el semáforo se elimina al llegar a cero
int ksem_close(ksem_t *sem) {
    if (sem == NULL) {
        return -1;
    }

    uint64_t flags = irq_save();
    if (sem->refcount == 0) {
        irq_restore(flags);
        return -1;
    }
    sem->refcount--;
    bool should_free = (sem->unlinked && sem->refcount == 0 && sem->wait_head == NULL);
    irq_restore(flags);

    if (should_free) {
        sem_free(sem);
    }
    return 0;
}

// Elimina el nombre del semáforo para futuras aperturas
int ksem_unlink(const char *name) {
    if (name == NULL) {
        return -1;
    }
    uint32_t len = sem_name_len(name);
    if (len == 0 || len >= KSEM_NAME_MAX) {
        return -1;
    }

    uint64_t flags = irq_save();
    uint32_t bucket = sem_hash(name);
    ksem_t *prev = NULL;
    ksem_t *cursor = sem_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && sem_name_cmp(cursor->name, name) == 0) {
            break;
        }
        prev = cursor;
        cursor = cursor->hash_next;
    }

    if (cursor == NULL) {
        irq_restore(flags);
        return -1;
    }

    if (prev == NULL) {
        sem_buckets[bucket] = cursor->hash_next;
    } else {
        prev->hash_next = cursor->hash_next;
    }
    cursor->hash_next = NULL;
    cursor->unlinked = true;
    bool should_free = (cursor->refcount == 0 && cursor->wait_head == NULL);

    irq_restore(flags);

    if (should_free) {
        sem_free(cursor);
    }
    return 0;
}

void ksem_remove_waiters_for(pcb_t *proc) {
    if (proc == NULL) {
        return;
    }

    sem_waiter_t *garbage = NULL;
    uint64_t flags = irq_save();

    for (int bucket = 0; bucket < KSEM_HASH_BUCKETS; bucket++) {
        ksem_t *sem = sem_buckets[bucket];
        while (sem != NULL) {
            sem_waiter_t *prev = NULL;
            sem_waiter_t *node = sem->wait_head;
            while (node != NULL) {
                if (node->proc == proc) {
                    sem_waiter_t *next = node->next;

                    if (prev == NULL) {
                        sem->wait_head = next;
                    } else {
                        prev->next = next;
                    }

                    if (sem->wait_tail == node) {
                        sem->wait_tail = prev;
                    }

                    node->next = garbage;
                    garbage = node;
                    node = next;
                    continue;
                }
                prev = node;
                node = node->next;
            }
            sem = sem->hash_next;
        }
    }

    irq_restore(flags);

    while (garbage != NULL) {
        sem_waiter_t *next = garbage->next;
        mm_free(garbage);
        garbage = next;
    }
}
